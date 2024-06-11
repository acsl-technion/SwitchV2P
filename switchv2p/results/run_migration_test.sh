#!/bin/bash

set -e

NS3_EXE=$NS3_HOME/ns3
RESULTS=$NS3_HOME/scratch/switchv2p/results/incast
PLACEMENT=$NS3_HOME/scratch/switchv2p/datasets/placement_n128_v80.json
TRACE=$NS3_HOME/scratch/switchv2p/datasets/incast.csv
MODES=("ondemand" "nocache" "switchv2p" "switchv2p_invalidations" "switchv2p_invalidations_bf")
get_cli () {
    local cli="$NS3_EXE run --no-build \"sim --udpMode --ports=8 --topology=Fattree --gatewayPerFlowLoadBalancing --gwLeaves=0,10,20,31 --P4SwitchApp::MemorySize=12 --P4SwitchApp::RandomHashFunction=false --P4SwitchApp::SourceLearning=true --P4SwitchApp::AccessBit=true --P4SwitchApp::GenerateProbability=0.005 --migration --migrationTs=500 --migrationContainerId=5202 --migrationDestinationLeaf=17 --migrationDestinationHost=0 --placement=$PLACEMENT --trace=$TRACE --output=$RESULTS/$1.json"
    case $1 in
        ondemand)
            cli="$cli --simMode=OnDemand"
            ;;
        nocache)
            cli="$cli --simMode=NoCache"
            ;;
        switchv2p)
            cli="$cli --P4SwitchApp::BloomFilter=false --simMode=SwitchV2P"
            ;;
        switchv2p_invalidations)
            cli="$cli --P4SwitchApp::BloomFilter=false --P4SwitchApp::GenerateInvalidations=true --simMode=SwitchV2P"
            ;;
        switchv2p_invalidations_bf)
            cli="$cli --P4SwitchApp::BloomFilter=true --P4SwitchApp::GenerateInvalidations=true --simMode=SwitchV2P"
            ;;
        *)
            exit 1
            ;;
    esac
    
    cli="$cli\" >& $RESULTS/$1.out"
    echo $cli 
}

if [[ ! -d $RESULTS ]]; then
    mkdir $RESULTS
fi

for mode in ${MODES[@]}; do
  echo "Running $mode"
  eval $(get_cli $mode)
done
