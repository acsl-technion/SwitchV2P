#ifndef TRACE_SIM_H
#define TRACE_SIM_H

#include "ns3/applications-module.h"
#include "sim-base.h"
#include "flow.h"
#include "migration-params.h"
#include "sim-parameters.h"
#include "packet-stats.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <set>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using std::set;
using std::string;
using std::unordered_map;
using std::vector;
using namespace boost::property_tree;
class TraceSimulation : public SimulationBase
{
public:
  TraceSimulation (string placementJsonPath, string traceCsvPath,
                   SimulationParameters simulationParameters, string outputPath,
                   MigrationParams migrationParams);

protected:
  virtual void Configure ();
  virtual void StopSimulation ();
  virtual void SetupApplications ();

private:
  uint64_t m_receivedPackets, m_sentPackets, m_gwPackets, m_lastGwPackets, m_generatedLearning,
      m_generatedInvalidation, m_droppedPackets, m_totalPacketLatency, m_totalPacketHops;
  void GatewayRx (Ptr<const Packet>, const Address &);
  void SinkRx (Ptr<const Packet>, const Address &, const Time &, const Time &, const uint32_t &);
  void ClientTx (Ptr<const Packet>, const uint32_t &);
  void GeneratedLearning (Ptr<const Packet>);
  void GeneratedInvalidation (Ptr<const Packet>);
  void ProcessedPacket (Ptr<const Packet>, uint32_t);
  void CacheHit (Ptr<const Packet> packet, uint32_t switchId);
  void RecordDropIp (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload,
                     Ipv4L3Protocol::DropReason reason, Ptr<Ipv4> ipv4, uint32_t ifIndex);
  void RecordDropQueue (Ptr<const Packet> packet);
  void RecordDropQueueDisc (Ptr<const QueueDiscItem> item);
  void RecordCongState (const TcpSocketState::TcpCongState_t, const TcpSocketState::TcpCongState_t,
                        const uint32_t &);

  void CalculateDestinations ();
  void GatewayMonitoring ();
  void Migration ();
  void UpdateMappings ();
  ptree CreatePtree (vector<uint32_t> &vec);
  template <typename T>
  ptree CreatePtree (unordered_map<uint32_t, T> &map);
  ptree CreatePtree (unordered_map<uint32_t, PacketStats> &stats);
  ContainerGroups ParsePlacement (string placementJsonPath,
                                  SimulationParameters simulationParameters);
  unordered_map<uint32_t, vector<Flow>> ParseTrace (string traceCsvPath);
  Time m_startTime, m_stopTime;
  unordered_map<uint32_t, vector<Flow>> m_containerToFlows;
  unordered_map<uint32_t, uint64_t> m_switchToProcessedPackets, m_switchToCacheHits,
      m_switchToProcessedBytes, m_switchToFirstCacheHits;
  unordered_map<uint32_t, FlowStats> m_flowStats;
  set<int> m_destinations;
  ApplicationContainer m_switchApps;
  string m_outputPath;
  vector<uint32_t> m_gatewayThroughput;
  MigrationParams m_migrationParams;
  uint32_t m_migrationSrcLeaf, m_migrationSrcHost;
};

#endif /* TRACE_SIM_H */