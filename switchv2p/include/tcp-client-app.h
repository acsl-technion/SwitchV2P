#ifndef TCP_CLIENT_APP_H
#define TCP_CLIENT_APP_H

#include "flow.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"

using namespace ns3;

class TcpClientApp : public Application
{
public:
  TcpClientApp ();

  /**
     * Register this type.
     * \return The TypeId.
     */
  static TypeId GetTypeId (void);
  void Setup (InetSocketAddress remoteAddress, InetSocketAddress LocalAddress,
              Ipv4Address sourceAddress, uint32_t maxBytes, uint32_t flowId);

protected:
  virtual void DoDispose ();

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
  void SendData (const Address &from, const Address &to);
  void ConnectionSucceeded (Ptr<Socket> socket);
  void ConnectionFailed (Ptr<Socket> socket);
  void DataSend (Ptr<Socket> socket, uint32_t unused);
  void TxEvent (const Ptr<const Packet> packet, const TcpHeader &header,
                const Ptr<const TcpSocketBase> socket);
  void RxEvent (const Ptr<const Packet> packet, const Address &from, const TcpHeader &header,
                const Ptr<const TcpSocketBase> socket);
  void RecordCongState (const TcpSocketState::TcpCongState_t, const TcpSocketState::TcpCongState_t,
                        const uint32_t &);

  Ptr<Socket> m_socket; //!< Associated socket
  Address m_peer; //!< Peer address
  Address m_local; //!< Local address to bind to
  bool m_connected; //!< True if connected
  uint64_t m_maxBytes; //!< Limit total number of bytes sent
  uint64_t m_totBytes; //!< Total bytes sent so far
  Ptr<Packet> m_unsentPacket; //!< Variable to cache unsent packet
  SourceTag m_sourceTag;
  TracedCallback<Ptr<const Packet>, const uint32_t &> m_txTrace;
  TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;
  TracedCallback<TcpSocketState::TcpCongState_t, TcpSocketState::TcpCongState_t, const uint32_t &>
      m_disorderTrace;
  uint32_t m_source;
  uint32_t m_flowId;
  uint32_t m_sendSize;
};

#endif /* TCP_CLIENT_APP_H */
