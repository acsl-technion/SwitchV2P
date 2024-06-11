#ifndef SIM_PARAMETERS_H
#define SIM_PARAMETERS_H

#include <boost/assign/list_of.hpp>
#include <map>
#include <string>
#include <vector>

using std::map;
using std::string;
using std::vector;

class SimulationParameters
{
public:
  enum Mode { Controller, SwitchV2P, LocalLearning, NoCache, GwCache, Direct, Bluebird, OnDemand };
  enum Topology { CLOS, FATTREE };

  SimulationParameters (string simMode, string networkTopology, size_t numOfPorts, size_t numOfCore,
                        size_t podWidth, vector<uint32_t> gatewayLeaves, bool randomRouting,
                        bool gatewayPerFlowLoadBalancing, bool udpMode);

  enum Mode SimMode;
  enum Topology NetworkTopology;
  size_t NumOfPorts;
  size_t NumOfCore;
  size_t PodWidth;
  vector<uint32_t> GatewayLeaves;
  bool RandomRouting;
  bool GatewayPerFlowLoadBalancing;
  bool UdpMode;

private:
  static const map<string, enum Mode> simulationModeMap;
  static const map<string, enum Topology> simulationTopologyMap;
};

#endif /* SIM_PARAMETERS_H */