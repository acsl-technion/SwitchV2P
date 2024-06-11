#include "include/socket-helper.h"
#include "include/ip-utils.h"
#include "include/invalidation-tag.h"
#include "include/hit-tag.h"

NS_LOG_COMPONENT_DEFINE ("SocketHelper");

const uint16_t SocketHelper::PORT_NUMBER = 667;

SocketHelper::SocketHelper (unordered_map<uint32_t, uint32_t> &virtualToPhysical,
                            uint32_t &misdeliveryCount, Time &lastMisdelivered,
                            bool gatewayPerFlowLoadBalancing, SimulationParameters simParams,
                            MigrationParams migrationParams)
    : m_virtualToPhysical (virtualToPhysical),
      m_misdeliveryCount (misdeliveryCount),
      m_lastMisdelivered (lastMisdelivered),
      m_gatewayPerFlowLoadBalancing (gatewayPerFlowLoadBalancing),
      m_firstPacket (true),
      m_simParams (simParams),
      m_migrationParams (migrationParams)
{
}

void
SocketHelper::SendFollowMeRule (Ptr<Packet> packet, Ipv4Header ipHeader, bool misdelivery)
{
  Ipv4Address dst =
      misdelivery
          ? IpUtils::GetNodePhysicalAddress (m_migrationParams.dstLeaf / m_simParams.PodWidth,
                                             m_migrationParams.dstLeaf % m_simParams.PodWidth,
                                             m_migrationParams.dstHost)
          : Ipv4Address (m_virtualToPhysical[ipHeader.GetDestination ().Get ()]);
  m_socket->SendTo (packet, 0, InetSocketAddress (dst, PORT_NUMBER));
}

void
SocketHelper::SendToGateway (Ptr<Packet> packet, uint32_t gwIdx)
{
  m_socket->SendTo (packet, 0, InetSocketAddress (m_gatewayAddresses[gwIdx], PORT_NUMBER));
}

uint32_t
SocketHelper::GetGatewayIdx (Ptr<Packet> packet, Ipv4Header &header)
{
  if (m_gatewayPerFlowLoadBalancing)
    {
      FlowIdTag flowTag;
      if (!packet->PeekPacketTag (flowTag))
        {
          NS_LOG_WARN ("Packet without a flow Id tag");
        }
      uint32_t flowId = flowTag.GetFlowId ();

      return CRC32Calculate ((uint8_t *) &flowId, sizeof (uint32_t)) % m_gatewayAddresses.size ();
    }

  return std::min (header.GetDestination ().Get () / m_gatewayRange,
                   m_gatewayAddresses.size () - 1);
}

bool
SocketHelper::VirtualSend (Ptr<Packet> packet, const Address &source, const Address &dest,
                           uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (*packet);

  if (m_migrationParams.migration && m_simParams.SimMode == SimulationParameters::OnDemand)
    {
      m_onDemandCache->insert (
          IpUtils::GetContainerVirtualAddress (m_migrationParams.containerId).Get ());
    }
  NS_LOG_DEBUG ("SH: Sending packet " << *packet);
  Ipv4Header header;
  packet->PeekHeader (header);
  NS_LOG_DEBUG ("Packet TTL = " << (uint32_t) header.GetTtl ());
  size_t gwIdx = GetGatewayIdx (packet, header);
  if (header.GetTtl () < 64)
    {
      InvalidationTag tag;
      tag.SetInvalidation (
          std::make_pair (header.GetDestination ().Get (), m_physicalAddress.Get ()));
      packet->AddPacketTag (tag);

      m_misdeliveryCount++;
      m_lastMisdelivered = Max (Simulator::Now (), m_lastMisdelivered);

      HitTag hitTag;
      if (packet->PeekPacketTag (hitTag))
        {
          Simulator::Schedule (MicroSeconds (10), &SocketHelper::SendToGateway, this, packet,
                               gwIdx);
        }
      else
        {
          // Use the follow-me rule to send the packet directly to its new destination
          Simulator::Schedule (MicroSeconds (10), &SocketHelper::SendFollowMeRule, this, packet,
                               header, true);
        }

      return true;
    }

  if (m_simParams.SimMode == SimulationParameters::Direct)
    {
      SendFollowMeRule (packet, header, false);
      return true;
    }

  if (m_simParams.SimMode == SimulationParameters::OnDemand &&
      m_onDemandCache->count (header.GetDestination ().Get ()))
    {
      if (m_firstPacket)
        {
          Simulator::Schedule (MicroSeconds (40), &SocketHelper::SendFollowMeRule, this, packet,
                               header, false);
        }
      else
        {
          SendFollowMeRule (packet, header, false);
        }
      m_firstPacket = false;
      return true;
    }

  NS_LOG_DEBUG ("SH: Virtual address = " << header.GetDestination ());
  m_socket->SendTo (packet, 0, InetSocketAddress (m_gatewayAddresses[gwIdx], PORT_NUMBER));
  return true;
}

void
SocketHelper::SocketRecv (Ptr<Socket> socket)
{
  Ptr<Packet> packet = socket->Recv (65535, 0);
  NS_LOG_DEBUG ("Socket recv: " << *packet);
  if (m_simParams.SimMode == SimulationParameters::OnDemand)
    {
      Ipv4Header header;
      packet->PeekHeader (header);
      m_onDemandCache->insert (header.GetSource ().Get ());
      m_onDemandCache->insert (header.GetDestination ().Get ());
    }
  m_vDev->Receive (packet, Ipv4L3Protocol::PROT_NUMBER, m_vDev->GetAddress (),
                   m_vDev->GetAddress (), NetDevice::PACKET_HOST);
}
