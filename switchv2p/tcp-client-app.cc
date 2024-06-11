#include "include/tcp-client-app.h"

NS_LOG_COMPONENT_DEFINE ("TcpClientApp");

TypeId
TcpClientApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("TcpClientApp")
                          .SetParent<Application> ()
                          .SetGroupName ("Sim")
                          .AddConstructor<TcpClientApp> ()
                          .AddTraceSource ("Tx", "A new packet is sent",
                                           MakeTraceSourceAccessor (&TcpClientApp::m_txTrace),
                                           "ns3::Packet::TracedCallback")
                          .AddTraceSource ("Rx", "A new packet is received",
                                           MakeTraceSourceAccessor (&TcpClientApp::m_rxTrace),
                                           "ns3::Packet::TracedCallback")
                          .AddTraceSource ("CongState", "The congestion state has changed",
                                           MakeTraceSourceAccessor (&TcpClientApp::m_disorderTrace),
                                           "ns3::TcpSocketState::TcpCongStatesTracedValueCallback");

  return tid;
}

TcpClientApp::TcpClientApp ()
    : m_socket (0),
      m_connected (false),
      m_totBytes (0),
      m_unsentPacket (0),
      m_flowId (0),
      m_sendSize (1024)
{
  NS_LOG_FUNCTION (this);
}

void
TcpClientApp::Setup (InetSocketAddress remoteAddress, InetSocketAddress localAddress,
                     Ipv4Address sourceAddress, uint32_t maxBytes, uint32_t flowId)
{
  NS_LOG_DEBUG ("Local address " << localAddress.GetIpv4 () << " Sending " << maxBytes
                                 << " of data to " << remoteAddress.GetIpv4 ()
                                 << ". Flow id = " << flowId);

  m_peer = remoteAddress;
  m_local = localAddress;
  m_sourceTag.SetSource (sourceAddress);
  m_source = sourceAddress.Get ();
  m_maxBytes = maxBytes;
  m_flowId = flowId;
}

void
TcpClientApp::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_socket = 0;
  m_unsentPacket = 0;
  // chain up
  Application::DoDispose ();
}

// Application Methods
void
TcpClientApp::StartApplication (void) // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  // Create the socket if not already
  if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      m_socket->SetAttribute ("Source", UintegerValue (m_source));
      m_socket->SetAttribute ("FlowId", UintegerValue (m_flowId));
      int ret = m_socket->Bind (m_local);

      if (ret == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }

      m_socket->TraceConnectWithoutContext ("Tx", MakeCallback (&TcpClientApp::TxEvent, this));
      m_socket->TraceConnectWithoutContext ("Rx", MakeCallback (&TcpClientApp::RxEvent, this));
      m_socket->TraceConnectWithoutContext ("CongState",
                                            MakeCallback (&TcpClientApp::RecordCongState, this));
      m_socket->Connect (m_peer);
      m_socket->ShutdownRecv ();
      m_socket->SetConnectCallback (MakeCallback (&TcpClientApp::ConnectionSucceeded, this),
                                    MakeCallback (&TcpClientApp::ConnectionFailed, this));
      m_socket->SetSendCallback (MakeCallback (&TcpClientApp::DataSend, this));
    }

  if (m_connected)
    {
      Address from;
      m_socket->GetSockName (from);
      SendData (from, m_peer);
    }
}

void
TcpClientApp::RecordCongState (const TcpSocketState::TcpCongState_t oldState,
                               const TcpSocketState::TcpCongState_t newState,
                               const uint32_t &flowId)
{
  m_disorderTrace (oldState, newState, flowId);
}

void
TcpClientApp::TxEvent (const Ptr<const Packet> packet, const TcpHeader &header,
                       const Ptr<const TcpSocketBase> socket)
{
  m_txTrace (packet, m_flowId);
}

void
TcpClientApp::RxEvent (const Ptr<const Packet> packet, const Address &from, const TcpHeader &header,
                       const Ptr<const TcpSocketBase> socket)
{
  m_rxTrace (packet, from);
}

void
TcpClientApp::StopApplication (void) // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      NS_LOG_DEBUG ("TcpClientAppClose: " << Simulator::Now ());
      m_socket->Close ();
      m_connected = false;
    }
  else
    {
      NS_LOG_WARN ("TcpClientApp found null socket to close in StopApplication");
    }
}

// Private helpers

void
TcpClientApp::SendData (const Address &from, const Address &to)
{
  NS_LOG_FUNCTION (this);

  while (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    { // Time to send more

      uint64_t toSend = m_sendSize;
      // Make sure we don't send too many
      if (m_maxBytes > 0)
        {
          toSend = std::min (toSend, m_maxBytes - m_totBytes);
        }

      NS_LOG_LOGIC ("Sending packet at " << Simulator::Now ());

      Ptr<Packet> packet;
      if (m_unsentPacket)
        {
          packet = m_unsentPacket;
          toSend = packet->GetSize ();
        }
      else
        {
          packet = Create<Packet> (toSend);
        }

      int actual = m_socket->Send (packet);
      if ((unsigned) actual == toSend)
        {
          m_totBytes += actual;
          m_unsentPacket = 0;
        }
      else if (actual == -1)
        {
          // We exit this loop when actual < toSend as the send side
          // buffer is full. The "DataSent" callback will pop when
          // some buffer space has freed up.
          NS_LOG_DEBUG ("Unable to send packet; caching for later attempt");
          m_unsentPacket = packet;
          break;
        }
      else if (actual > 0 && (unsigned) actual < toSend)
        {
          // A Linux socket (non-blocking, such as in DCE) may return
          // a quantity less than the packet size.  Split the packet
          // into two, trace the sent packet, save the unsent packet
          NS_LOG_DEBUG ("Packet size: " << packet->GetSize () << "; sent: " << actual
                                        << "; fragment saved: " << toSend - (unsigned) actual);
          Ptr<Packet> sent = packet->CreateFragment (0, actual);
          Ptr<Packet> unsent = packet->CreateFragment (actual, (toSend - (unsigned) actual));
          m_totBytes += actual;
          m_unsentPacket = unsent;
          break;
        }
      else
        {
          NS_FATAL_ERROR ("Unexpected return value from m_socket->Send ()");
        }
    }

  if (m_totBytes == m_maxBytes && m_connected)
    {
      NS_LOG_DEBUG (InetSocketAddress::ConvertFrom (m_local).GetIpv4 ()
                    << ": TcpClientApp finished sending data. Closing socket. "
                    << Simulator::Now ());
      m_socket->Close ();
      m_connected = false;
    }
}

void
TcpClientApp::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("TcpClientApp Connection succeeded");
  m_connected = true;
  Address from, to;
  socket->GetSockName (from);
  socket->GetPeerName (to);
  SendData (from, to);
}

void
TcpClientApp::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("TcpClientApp, Connection Failed");
}

void
TcpClientApp::DataSend (Ptr<Socket> socket, uint32_t)
{
  NS_LOG_FUNCTION (this);

  if (m_connected)
    { // Only send new data if the connection has completed
      Address from, to;
      socket->GetSockName (from);
      socket->GetPeerName (to);
      SendData (from, to);
    }
}
