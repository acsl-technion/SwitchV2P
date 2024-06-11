#include "include/client-app.h"
#include "include/ip-utils.h"
#include "ns3/nstime.h"
#include "include/timestamp-tag.h"
#include "include/tcp-client-helper.h"
#include "ns3/hops-tag.h"

NS_LOG_COMPONENT_DEFINE ("ClientApp");

const uint16_t CLIENT_PORT = 9;

ClientApp::ClientApp ()
    : m_socket (0), m_sendEvent (), m_running (false), m_currentIdx (0), m_basePort (CLIENT_PORT)
{
}

/**
 * Register this type.
 * \return The TypeId.
 */
TypeId
ClientApp::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ClientApp")
          .SetParent<Application> ()
          .SetGroupName ("Sim")
          .AddConstructor<ClientApp> ()
          .AddAttribute ("PacketSize", "Size of the outbound packets", UintegerValue (64),
                         MakeUintegerAccessor (&ClientApp::m_packetSize),
                         MakeUintegerChecker<uint16_t> ())
          .AddTraceSource ("Tx", "A new packet is created and is sent",
                           MakeTraceSourceAccessor (&ClientApp::m_txTrace),
                           "ns3::Packet::TracedCallback")
          .AddTraceSource ("RxWithDelay", "A packet has been received with its delay",
                           MakeTraceSourceAccessor (&ClientApp::m_rxTraceWithDelay),
                           "ns3::Packet::DelayTracedCallback")
          .AddTraceSource ("CongState", "The congestion state has changed",
                           MakeTraceSourceAccessor (&ClientApp::m_disorderTrace),
                           "ns3::TcpSocketState::TcpCongStatesTracedValueCallback");

  return tid;
}

void
ClientApp::Setup (vector<Flow> *flows, Ipv4Address address, Ipv4Address physicalAddress,
                  bool udpMode)
{
  m_flows = flows;
  m_address = address;
  m_sourcePhysicalAddress = physicalAddress;
  m_sourceTag.SetSource (physicalAddress);
  m_udpMode = udpMode;
}

void
ClientApp::StartApplication (void)
{
  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
    }
  m_running = true;
  if (m_udpMode)
    {
      m_socket->Bind (InetSocketAddress (m_address));
    }
  NS_LOG_DEBUG ("Running client with " << m_flows->size () << " flows");
  ScheduleNext ();
}

void
ClientApp::ScheduleNext ()
{
  uint64_t dt = m_currentIdx == 0
                    ? m_flows->at (0).ts
                    : m_flows->at (m_currentIdx).ts - m_flows->at (m_currentIdx - 1).ts;
  m_sendEvent = Simulator::Schedule (NanoSeconds (dt), &ClientApp::SendPacket, this);
}

void
ClientApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

/// Send a packet.
void
ClientApp::SendPacket (void)
{
  if (m_udpMode)
    {
      Ptr<Packet> packet = Create<Packet> (m_packetSize);
      Ipv4Address dstAddr = IpUtils::GetContainerVirtualAddress (m_flows->at (m_currentIdx).dst);
      DelayJitterEstimation::PrepareTx (packet);
      packet->AddPacketTag (TimestampTag ());
      packet->AddPacketTag (FlowIdTag (m_flows->at (m_currentIdx).flowId));
      packet->AddPacketTag (m_sourceTag);
      packet->AddPacketTag (HopsTag ());
      m_txTrace (packet, m_flows->at (m_currentIdx).flowId);
      m_socket->SendTo (packet, 0, InetSocketAddress (dstAddr, CLIENT_PORT));
      NS_LOG_INFO ("At time " << Simulator::Now ().As (Time::S) << " client sent " << m_packetSize
                              << " bytes to " << dstAddr << " port " << CLIENT_PORT);
    }
  else
    {
      Ipv4Address dstAddr = IpUtils::GetContainerVirtualAddress (m_flows->at (m_currentIdx).dst);
      TcpClientAppHelper helper (InetSocketAddress (dstAddr, CLIENT_PORT),
                                 InetSocketAddress (m_address, 0), m_sourcePhysicalAddress,
                                 m_flows->at (m_currentIdx).size,
                                 m_flows->at (m_currentIdx).flowId);
      ApplicationContainer bulkApps = helper.Install (GetNode ());
      bulkApps.Get (0)->TraceConnectWithoutContext ("Tx", MakeCallback (&ClientApp::TxEvent, this));
      bulkApps.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&ClientApp::RxEvent, this));
      bulkApps.Get (0)->TraceConnectWithoutContext (
          "CongState", MakeCallback (&ClientApp::RecordCongState, this));

      bulkApps.Start (NanoSeconds (0));
    }

  ++m_currentIdx;
  if (m_currentIdx < m_flows->size ())
    ScheduleNext ();
}

void
ClientApp::TxEvent (Ptr<const Packet> packet, const uint32_t &flowId)
{
  m_txTrace (packet, flowId);
}

void
ClientApp::RxEvent (Ptr<const Packet> packet, const Address &from)
{
  m_delayEstimation.RecordRx (packet);
  FlowIdTag flowIdTag;
  packet->PeekPacketTag (flowIdTag);
  m_rxTraceWithDelay (packet, from, Simulator::Now () - m_delayEstimation.GetLastDelay (),
                      Simulator::Now (), flowIdTag.GetFlowId ());
}

void
ClientApp::RecordCongState (const TcpSocketState::TcpCongState_t oldState,
                            const TcpSocketState::TcpCongState_t newState, const uint32_t &flowId)
{
  m_disorderTrace (oldState, newState, flowId);
}
