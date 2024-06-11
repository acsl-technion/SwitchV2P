#ifndef GATEWAY_APP_H
#define GATEWAY_APP_H

#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include <unordered_map>

using namespace ns3;
using std::unordered_map;

class GatewayApp : public Application
{
public:
  GatewayApp ();

  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId (void);
  void Setup (unordered_map<uint32_t, uint32_t>* virtualToPhysical);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  /// Send a packet.
  void ReceivePacket (Ptr<Socket> socket);
  void SendPacket (Ptr<Socket> socket, Ptr<Packet> packet, Ipv4Header innerHeader);

  Ptr<Socket> m_socket; //!< The tranmission socket.
  unordered_map<uint32_t, uint32_t> *m_virtualToPhysical;
  TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;
};

#endif /* GATEWAY_APP_H */