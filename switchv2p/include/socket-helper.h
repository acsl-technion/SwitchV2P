#ifndef SOCKET_HELPER_H
#define SOCKET_HELPER_H

#include <vector>
#include <unordered_map>
#include <set>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/virtual-net-device-module.h"
#include "sim-parameters.h"
#include "migration-params.h"

using namespace ns3;
using std::set;
using std::unordered_map;
using std::vector;

class SocketHelper
{
private:
  void SendFollowMeRule (Ptr<Packet> packet, Ipv4Header ipHeader, bool misdelivery);
  void SendToGateway (Ptr<Packet> packet, uint32_t gwIdx);
  uint32_t GetGatewayIdx (Ptr<Packet> packet, Ipv4Header &header);

public:
  static const uint16_t PORT_NUMBER;
  SocketHelper (unordered_map<uint32_t, uint32_t> &virtualToPhysical, uint32_t &misdeliveryCount,
                Time &lastMisdelivered, bool gatewayPerFlowLoadBalancing,
                SimulationParameters simParams, MigrationParams migrationParams);
  bool VirtualSend (Ptr<Packet> packet, const Address &source, const Address &dest,
                    uint16_t protocolNumber);
  void SocketRecv (Ptr<Socket> socket);

  Ptr<Node> m_node;
  Ptr<Socket> m_socket;
  Ptr<VirtualNetDevice> m_vDev;
  unordered_map<uint32_t, uint32_t> &m_virtualToPhysical;
  uint32_t &m_misdeliveryCount;
  Time &m_lastMisdelivered;
  set<uint32_t> *m_onDemandCache;
  vector<Ipv4Address> m_gatewayAddresses;
  size_t m_gatewayRange;
  bool m_gatewayPerFlowLoadBalancing, m_firstPacket;
  SimulationParameters m_simParams;
  MigrationParams m_migrationParams;

  Ipv4Address m_physicalAddress;
};

#endif /* SOCKET_HELPER_H */
