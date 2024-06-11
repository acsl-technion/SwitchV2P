#ifndef SWITCH_APP_H
#define SWITCH_APP_H

#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "lru-cache.h"
#include "flow-info.h"
#include "sim-parameters.h"
#include <unordered_map>
#include <set>

using namespace ns3;
using std::pair;
using std::set;
using std::unordered_map;
using std::vector;

// src -> { flowId -> FlowInfo }
typedef unordered_map<uint32_t, unordered_map<uint32_t, FlowInfo>> TrafficMatrix;

class SwitchApp : public Application
{
public:
  SwitchApp ();

  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId (void);
  void Setup (vector<Ipv4Address> &gwAddresses, enum SimulationParameters::Mode simMode);
  void BulkInsertToCache (vector<pair<uint32_t, uint32_t>> &items);
  TrafficMatrix GetTrafficMatrix ();

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  /// Send a packet.
  bool ReceivePacket (Ptr<Packet> packet, Ipv4Header &ipHeader);

  LRUCache<uint32_t, uint32_t> m_cache;
  set<uint32_t> m_gwAddresses;
  TrafficMatrix m_trafficMatrix;
  enum SimulationParameters::Mode m_switchMode;
  int m_memorySize;
  TracedCallback<Ptr<const Packet>, uint32_t> m_processedPackets, m_cacheHit;
};

#endif /* SWITCH_APP_H */