#include "include/switch-app.h"
#include "ns3/internet-module.h"

NS_LOG_COMPONENT_DEFINE ("SwitchApp");
NS_OBJECT_ENSURE_REGISTERED (SwitchApp);

/**
   * Register this type.
   * \return The TypeId.
   */
TypeId
SwitchApp::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("SwitchApp")
          .SetParent<Application> ()
          .SetGroupName ("Sim")
          .AddConstructor<SwitchApp> ()
          .AddAttribute ("MemorySize", "The number of entries each switch can store",
                         IntegerValue (10), MakeIntegerAccessor (&SwitchApp::m_memorySize),
                         MakeIntegerChecker<int32_t> ())
          .AddTraceSource ("ProcessedPackets", "A packet has been processed by the switch",
                           MakeTraceSourceAccessor (&SwitchApp::m_processedPackets),
                           "ns3::Packet::SwitchIdTracedCallback")
          .AddTraceSource ("CacheHit", "A packet hit the cache on the switch",
                           MakeTraceSourceAccessor (&SwitchApp::m_cacheHit),
                           "ns3::Packet::SwitchIdTracedCallback");

  return tid;
}

SwitchApp::SwitchApp ()
{
}

void
SwitchApp::Setup (vector<Ipv4Address> &gwAddresses, enum SimulationParameters::Mode switchMode)
{
  std::transform (gwAddresses.begin (), gwAddresses.end (),
                  inserter (m_gwAddresses, m_gwAddresses.end ()),
                  [] (const Ipv4Address &ipv4) { return ipv4.Get (); });
  m_switchMode = switchMode;
  m_cache.SetCapacity (m_memorySize);
}

void
SwitchApp::StartApplication (void)
{
  Ptr<Ipv4L3Protocol> ipv4Proto = m_node->GetObject<Ipv4L3Protocol> ();
  if (ipv4Proto != 0)
    {
      ipv4Proto->AddPacketInterceptor (MakeCallback (&SwitchApp::ReceivePacket, this),
                                       UdpL4Protocol::PROT_NUMBER);
    }
}

void
SwitchApp::StopApplication (void)
{
}

TrafficMatrix
SwitchApp::GetTrafficMatrix ()
{
  TrafficMatrix trafficMatrix = m_trafficMatrix;
  m_trafficMatrix.clear ();
  return trafficMatrix;
}

void
SwitchApp::BulkInsertToCache (vector<pair<uint32_t, uint32_t>> &items)
{
  for (auto &pair : items)
    {
      NS_LOG_INFO ("Inserting [" << Ipv4Address (pair.first) << ", " << Ipv4Address (pair.second)
                                 << "]");
      m_cache.Put (pair.first, pair.second);
    }
}

/// Send a packet.
bool
SwitchApp::ReceivePacket (Ptr<Packet> packet, Ipv4Header &ipHeader)
{
  HopsTag hopsTag;
  if (packet->RemovePacketTag (hopsTag))
    {
      hopsTag.IncHops ();
      packet->AddPacketTag (hopsTag);
    }

  Ptr<Packet> receivedPacket = packet->Copy ();
  m_processedPackets (receivedPacket, GetNode ()->GetId ());
  receivedPacket->AddHeader (ipHeader);

  UdpHeader udpHeader;
  packet->RemoveHeader (udpHeader);
  Ipv4Header innerHeader;
  packet->RemoveHeader (innerHeader);

  packet->AddHeader (innerHeader);
  packet->AddHeader (udpHeader);

  if (m_switchMode == SimulationParameters::Mode::Controller)
    {
      FlowIdTag flowTag;
      packet->PeekPacketTag (flowTag);
      uint32_t flowId = flowTag.GetFlowId ();

      if (m_gwAddresses.count (ipHeader.GetDestination ().Get ()))
        {
          m_trafficMatrix[innerHeader.GetSource ().Get ()][flowId].dstIp =
              innerHeader.GetDestination ().Get ();
          m_trafficMatrix[innerHeader.GetSource ().Get ()][flowId].gwIp =
              ipHeader.GetDestination ().Get ();
          m_trafficMatrix[innerHeader.GetSource ().Get ()][flowId].packetCount += 1;
        }
    }

  if (m_gwAddresses.count (ipHeader.GetDestination ().Get ()) != 0)
    {
      uint32_t cached_addr = 0;
      if (m_cache.Get (innerHeader.GetDestination ().Get (), cached_addr))
        {
          m_cacheHit (receivedPacket, GetNode ()->GetId ());
          ipHeader.SetDestination (Ipv4Address (cached_addr));
        }
    }

  return true;
}
