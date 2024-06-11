#ifndef P4_SWITCH_APP_H
#define P4_SWITCH_APP_H

#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "lru-cache.h"
#include "p4-cache.h"
#include "bloom-filter.h"
#include "sim-parameters.h"
#include <set>
#include <unordered_map>
#include <vector>

using namespace ns3;
using std::set;
using std::unordered_map;
using std::vector;

class P4SwitchApp : public Application
{
public:
  enum SwitchType { LEAF, GW_LEAF, GW_SPINE, SPINE, CORE };
  P4SwitchApp ();

  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId (void);
  void Setup (vector<Ipv4Address> &gwAddresses, Ipv4Address switchAddress,
              enum SwitchType switchType, enum SimulationParameters::Mode simMode,
              uint32_t podCount, unordered_map<uint32_t, uint32_t> *virtualToPhysical);

private:
  static const uint16_t SWITCH_PORT;
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  bool ReceivePacket (Ptr<Packet> packet, Ipv4Header &ipHeader);
  bool HandleProtocolPacket (Ptr<Packet> packet, Ipv4Header &ipHeader);
  bool LocalP4CacheLogic (uint32_t virtualDestinationIp, uint32_t physicalDestinationIp,
                          Ptr<Packet> packet, Ipv4Header &ipHeader);
  void BluebirdProcessPacket (Ptr<Packet> packet);

  void PopulateBluebirdCache (uint32_t virtualIp);
  void BluebirdTxStart (Ptr<Packet> packet);
  void BluebirdTxComplete ();
  void ReceiveRawPacket (Ptr<Socket> socket);

  bool BluebirdLogic (uint32_t virtualDestinationIp, uint32_t physicalDestinationIp,
                      Ptr<Packet> packet, Ipv4Header &ipHeader);
  void GeneratePacketToLeaf (pair<uint32_t, uint32_t> data, Ipv4Address hostAddress);
  void GeneratePacketToAddress (pair<uint32_t, uint32_t> data, Ipv4Address dstAddress,
                                bool learning);

  QueueSize m_bluebirdQueueSize;
  Ptr<Queue<Packet>> m_bluebirdQueue;
  DataRate m_bps;
  Time m_bluebirdDelay, m_bluebirdProgrammingDelay;
  LRUCache<uint32_t, uint32_t> m_bluebirdCache;
  P4Cache<uint32_t, uint32_t> m_cache;
  BloomFilter<uint32_t> m_bloomFilter;
  set<uint32_t> m_gwAddresses;
  int m_memorySize, m_bloomFilterSize;
  enum SwitchType m_switchType;
  Ipv4Address m_switchAddress;
  bool m_randomHash, m_sourceLearning, m_accessBit, m_bloomFilterEnabled, m_generateInvalidation,
      m_bluebirdBusy;
  enum SimulationParameters::Mode m_simMode;
  Ptr<Socket> m_socket;
  vector<Ptr<Socket>> m_bluebirdSockets;
  Ptr<UniformRandomVariable> m_random;
  uint32_t m_podCount, m_defaultTtl;
  double m_generateProb;
  TracedCallback<Ptr<const Packet>> m_learningTrace, m_invalidationTrace;
  TracedCallback<Ptr<const Packet>, uint32_t> m_processedPackets, m_cacheHit;
  unordered_map<uint32_t, uint32_t> *m_virtualToPhysical;
  unordered_map<uint64_t, Ptr<Socket>> m_packetToSocket;
};

#endif /* P4_SWITCH_APP_H */