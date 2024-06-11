#ifndef CLIENT_APP_H
#define CLIENT_APP_H

#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "flow.h"
#include <vector>

using namespace ns3;
using std::vector;

class ClientApp : public Application
{
public:
  ClientApp ();

  /**
     * Register this type.
     * \return The TypeId.
     */
  static TypeId GetTypeId (void);
  static const uint16_t CLIENT_PORT = 9;
  void Setup (vector<Flow> *flows, Ipv4Address address, Ipv4Address physicalSource,
              bool udpMode);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
  void ScheduleNext ();

  /// Send a packet.
  void SendPacket (void);

  Ptr<Socket> m_socket; //!< The tranmission socket.
  EventId m_sendEvent; //!< Send event.
  bool m_running, m_udpMode; //!< True if the application is running.
  vector<Flow> *m_flows;
  Ipv4Address m_address, m_sourcePhysicalAddress;
  TracedCallback<Ptr<const Packet>, const uint32_t &> m_txTrace;
  /// Callbacks for tracing the packet Rx events, includes source address, and delay
  TracedCallback<Ptr<const Packet>, const Address &, const Time &, const Time &, const uint32_t &>
      m_rxTraceWithDelay;
  TracedCallback<TcpSocketState::TcpCongState_t, TcpSocketState::TcpCongState_t, const uint32_t &>
      m_disorderTrace;

  size_t m_currentIdx;
  SourceTag m_sourceTag;
  uint16_t m_packetSize, m_basePort;
  DelayJitterEstimation m_delayEstimation;
  void TxEvent (Ptr<const Packet>, const uint32_t &flowId);
  void RxEvent (Ptr<const Packet>, const Address &);
  void RecordCongState (const TcpSocketState::TcpCongState_t, const TcpSocketState::TcpCongState_t,
                        const uint32_t &);
};

#endif /* CLIENT_APP_H */
