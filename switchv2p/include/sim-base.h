#ifndef SIM_BASE_H
#define SIM_BASE_H

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "socket-helper.h"
#include "sim-parameters.h"
#include "migration-params.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <set>

using std::pair;
using std::set;
using std::string;
using std::unordered_map;
using std::vector;
using namespace ns3;

typedef vector<vector<vector<string>>> ContainerGroups;

class SimulationBase
{
public:
  SimulationBase (SimulationParameters simParameters, ContainerGroups containerGroups,
                  MigrationParams migParams);

  virtual void Run ();

protected:
  virtual void AssignIds ();

  virtual void Configure ();

  virtual void InitializeNodes ();

  virtual void InstallNetworkStack ();

  virtual void CreateTopology ();

  virtual void AssignPhysicalIps ();

  virtual void SetupTunnel ();

  virtual void SetupApplications () = 0;

  virtual void StartSimulation ();

  virtual void StopSimulation ();

  ContainerGroups m_containerGroups;
  uint32_t m_leafCount, m_spineCount, m_coreCount, m_podWidth, m_podCount, m_misdeliveryCount;
  Time m_lastMisdelivered;
  PointToPointHelper m_nodeToSw;
  PointToPointHelper m_swToSw;
  NodeContainer m_cores, m_spines, m_leaves;
  vector<NodeContainer> m_nodes;
  Ipv4AddressHelper m_address;
  vector<vector<NetDeviceContainer>> m_nodeToSwDevice, m_swToSwDevice, m_coreToSpineDevice;
  unordered_map<uint32_t, uint32_t> m_virtualToPhysical;
  vector<vector<set<uint32_t>>> m_onDemandCaches;
  vector<vector<SocketHelper>> m_socketHelpers;
  AsciiTraceHelper m_asciiHelper;
  Ptr<OutputStreamWrapper> m_traceStream;
  unordered_map<string, uint32_t> m_containerToId;
  SimulationParameters m_simParameters;
  vector<pair<uint32_t, uint32_t>> m_gws;
  vector<Ipv4Address> m_gwAddresses;
};

#endif /* SIM_BASE_H */