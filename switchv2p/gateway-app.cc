#include "include/gateway-app.h"
#include "include/eviction-tag.h"
#include "include/invalidation-tag.h"

NS_LOG_COMPONENT_DEFINE ("GatewayApp");

GatewayApp::GatewayApp () : m_socket (0)
{
}

/**
   * Register this type.
   * \return The TypeId.
   */
TypeId
GatewayApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("GatewayApp")
                          .SetParent<Application> ()
                          .SetGroupName ("Sim")
                          .AddConstructor<GatewayApp> ()
                          .AddTraceSource ("Rx", "A packet has been received",
                                           MakeTraceSourceAccessor (&GatewayApp::m_rxTrace),
                                           "ns3::Packet::AddressTracedCallback");
  return tid;
}

void
GatewayApp::Setup (unordered_map<uint32_t, uint32_t> *virtualToPhysical)
{
  m_virtualToPhysical = virtualToPhysical;
}

void
GatewayApp::StartApplication (void)
{
  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::Ipv4RawSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      m_socket->SetAttribute ("Protocol", UintegerValue (UdpL4Protocol::PROT_NUMBER));
      m_socket->SetAttribute ("IpHeaderInclude", BooleanValue (true));
      m_socket->BindToNetDevice (GetNode ()->GetDevice (1));
      m_socket->SetRecvCallback (MakeCallback (&GatewayApp::ReceivePacket, this));
    }
}
void
GatewayApp::StopApplication (void)
{
  if (m_socket)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
      m_socket = 0;
    }
}

void
GatewayApp::SendPacket (Ptr<Socket> socket, Ptr<Packet> packet, Ipv4Header innerHeader)
{
  EvictionTag tag;
  packet->RemovePacketTag (tag);
  InvalidationTag invalidationTag;
  packet->RemovePacketTag (invalidationTag);
  socket->Send (packet);
  m_rxTrace (packet, innerHeader.GetDestination ());
}

void
GatewayApp::ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  while ((packet = socket->Recv ()))
    {
      NS_LOG_INFO ("Received a new packet at gateway: " << *packet);
      Ipv4Header outterHeader; // Physical IPs
      packet->RemoveHeader (outterHeader);
      UdpHeader udpHeader;
      packet->RemoveHeader (udpHeader);
      Ipv4Header innerHeader; // Virtual IPs
      packet->RemoveHeader (innerHeader);
      if (m_virtualToPhysical->count (innerHeader.GetDestination ().Get ()) == 0)
        {
          NS_LOG_ERROR ("Failed to find vip: " << innerHeader.GetDestination ().Get ());
        }

      uint32_t physical_addr_int = m_virtualToPhysical->at (innerHeader.GetDestination ().Get ());
      outterHeader.SetDestination (Ipv4Address (physical_addr_int));
      packet->AddHeader (innerHeader);
      packet->AddHeader (udpHeader);
      packet->AddHeader (outterHeader);
      Simulator::Schedule (MicroSeconds (40), &GatewayApp::SendPacket, this, socket, packet,
                           innerHeader);
    }
}
