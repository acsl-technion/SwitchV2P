#include "include/sim-parameters.h"

const map<string, enum SimulationParameters::Mode> SimulationParameters::simulationModeMap =
    boost::assign::map_list_of ("Controller", Controller) ("SwitchV2P", SwitchV2P) (
        "GwCache", GwCache) ("LocalLearning", LocalLearning) ("NoCache", NoCache) (
        "Direct", Direct) ("Bluebird", Bluebird) ("OnDemand", OnDemand);

const map<string, enum SimulationParameters::Topology> SimulationParameters::simulationTopologyMap =
    boost::assign::map_list_of ("Clos", CLOS) ("Fattree", FATTREE);

SimulationParameters::SimulationParameters (string simMode, string networkTopology,
                                            size_t numOfPorts, size_t numOfCore, size_t podWidth,
                                            vector<uint32_t> gatewayLeaves, bool randomRouting,
                                            bool gatewayPerFlowLoadBalancing, bool udpMode)
    : SimMode (SimulationParameters::simulationModeMap.at (simMode)),
      NetworkTopology (SimulationParameters::simulationTopologyMap.at (networkTopology)),
      NumOfPorts (numOfPorts),
      NumOfCore (numOfCore),
      PodWidth (podWidth),
      GatewayLeaves (gatewayLeaves),
      RandomRouting (randomRouting),
      GatewayPerFlowLoadBalancing (gatewayPerFlowLoadBalancing),
      UdpMode (udpMode)
{
}
