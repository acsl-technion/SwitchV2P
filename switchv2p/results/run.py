#!/usr/bin/python3

import sys
import subprocess
import os
import itertools
import json
import numpy as np
import argparse

ADDRESS_SPACE_SIZE = {"alibaba": 410796, "hadoop": 80 * 128, "websearch": 80 * 128, "microburst": 80 * 128, "video": 80 * 128}

SWITCH_COUNT = {"alibaba": 816, "hadoop": 80, "websearch": 80, "microburst": 80, "video": 80}
NS3_HOME = os.environ['NS3_HOME']
MAX_PROC = 30 # Set to 10 for Alibaba and Controller.
PLACEMENT = {
    "hadoop": os.path.join(
        NS3_HOME, "scratch/switchv2p/datasets/placement_n128_v80.json"
    ),
    "microburst": os.path.join(
        NS3_HOME, "scratch/switchv2p/datasets/placement_n128_v80.json"
    ),
    "video": os.path.join(
        NS3_HOME, "scratch/switchv2p/datasets/placement_n128_v80.json"
    ),
    "websearch": os.path.join(
        NS3_HOME, "scratch/switchv2p/datasets/placement_n128_v80.json"
    ),
    "alibaba": os.path.join(
        NS3_HOME, "scratch/switchv2p/datasets/placement_alibaba.json"
    ),
}
TRACE = {
    "hadoop": os.path.join(
        NS3_HOME, "scratch/switchv2p/datasets/hadoop.csv"
    ),
    "microburst": os.path.join(
        NS3_HOME, "scratch/switchv2p/datasets/microburst.csv"
    ),
    "video": os.path.join(
        NS3_HOME, "scratch/switchv2p/datasets/video.csv"
    ),
    "websearch": os.path.join(
        NS3_HOME, "scratch/switchv2p/datasets/websearch.csv"
    ),
    "alibaba": os.path.join(
        NS3_HOME, "scratch/switchv2p/datasets/alibaba.csv"),
}


def get_next_experiment_num(directory):
    subdirs = os.listdir(directory)
    if len(subdirs) == 0:
        return 0 

    return max([int(d) for d in subdirs]) + 1


def dict_product(dicts):
    return (dict(zip(dicts, x)) for x in itertools.product(*dicts.values()))


def generate_configs(simMode, workload, gwScaling, topoScaling):
    if gwScaling:
        assert workload == "hadoop"
        config_options = {
            "gwLeaves": [','.join(np.repeat(['0', '10', '20', '31'], x)) for x in [1, 3, 5, 10]] 
        }
    elif topoScaling:
        assert workload == "hadoop"
        config_options = {
            "gwLeaves":
              [",".join(["0"] * 10 + ["10"] * 10 + ["20"] * 10 + ["127"] * 10)]
            + [",".join(["0"] * 10 + ["10"] * 10 + ["20"] * 10 + ["63"] * 10)]
            + [",".join(["0"] * 10 + ["10"] * 10 + ["20"] * 10 + ["31"] * 10)]
            + [",".join(["0"] * 10 + ["1"] * 10 + ["2"] * 10 + ["3"] * 10)]
        }
    else:
        config_options = (
            {
                "gwLeaves": [
                    ",".join(["0"] * 10 + ["10"] * 10 + ["20"] * 10 + ["31"] * 10)
                ]
            }
            if workload != "alibaba"
            else {"gwLeaves": [",".join([str(x * 8 + 7) for x in range(0, 49, 2) for _ in range(10)])]}
        )
    if simMode != "GwCache":
        if simMode == "LocalLearning":
            config_options["randomHashFunction"] = ["true"]
        if simMode == "SwitchV2P":
            config_options["randomHashFunction"] = ["false"]
            config_options["sourceLearning"] = ["true"]
            config_options["accessBit"] = ["true"]
            config_options["generateProbability"] = [0.005]

    return list(dict_product(config_options))


def get_sim_modes():
    return [
        "SwitchV2P",
        "LocalLearning",
        "GwCache",
        "NoCache",
        "Bluebird",
        "Direct",
        "OnDemand",
        "Controller-150",
        "Controller-300",
    ]

def get_command_line(simMode, workload, config, output_file, topoScaling, gwScaling):
    cli = '"sim '
    if workload in ['microburst', 'video']:
        cli += '--udpMode '
    cli += '--gatewayPerFlowLoadBalancing --ports={} --podWidth={} --topology=Fattree --gwLeaves={} --IlpControllerApp::Interval={} --IlpControllerApp::MemorySize={} --SwitchApp::MemorySize={} --P4SwitchApp::MemorySize={} --P4SwitchApp::RandomHashFunction={} --P4SwitchApp::SourceLearning={} --P4SwitchApp::AccessBit={} --P4SwitchApp::GenerateProbability={} --simMode={}  --placement={} --trace={} --output={}"'

    ports = 8
    podWidth = 4
    interval = f"{simMode.split('-')[1]}us" if "Controller" in simMode else "100us"
    gwCount = len(set(config["gwLeaves"].split(",")))
    if not topoScaling:
        podCount = gwCount * 2

    else:
        if '3' in config["gwLeaves"].split(','):
            podCount = 1
        elif '31' in config["gwLeaves"].split(','):
            podCount = 8
        elif '63' in config["gwLeaves"].split(','):
            podCount = 16
        else:
            assert '127' in config["gwLeaves"]
            podCount = 32

    if workload == "alibaba":
        ports = 64
        podWidth = 8

    if simMode == "GwCache":
        per_switch_memory = config["mem_size"] // gwCount
    elif simMode == "Bluebird":
        per_switch_memory = config["mem_size"] // (podWidth * podCount)
    elif not topoScaling:
        per_switch_memory = config["mem_size"] // SWITCH_COUNT[workload]
    else:
        assert simMode != "GwCache" and topoScaling
        switchCount = (2 * podWidth) * podCount
        if podCount != 1:
            switchCount += 16
        per_switch_memory = config["mem_size"] // switchCount

    config["per_switch_memory"] = per_switch_memory

    if topoScaling:
        config["pod_count"] = podCount
        ports = 128 // (podCount * podWidth) * 2

    if gwScaling:
        config['gw_count'] = len(config['gwLeaves'].split(','))


    return cli.format(
        ports,
        podWidth,
        config["gwLeaves"],
        interval,
        per_switch_memory,
        per_switch_memory,
        per_switch_memory,
        config.get("randomHashFunction", "false"),
        config.get("sourceLearning", "false"),
        config.get("accessBit", "false"),
        config.get("generateProbability", 0.1),
        "Controller" if "Controller" in simMode else simMode,
        PLACEMENT[workload],
        TRACE[workload],
        output_file,
    )

def create_dir_if_not_exists(dirName):
    __location__ = os.path.realpath(
        os.path.join(os.getcwd(), os.path.dirname(__file__))
    )
    dirPath = os.path.join(__location__, dirName)
    if not os.path.exists(dirPath):
        os.makedirs(dirPath)
    
    return dirPath



def get_default_output_dir(workload, topoScaling, gwScaling):
    if topoScaling:
        dirName = f"{workload}_toposcaling"
    elif gwScaling:
        dirName = f"{workload}_gwscaling"
    else:
        dirName = f"{workload}"

    return create_dir_if_not_exists(dirName)

def run_workload(workload, gwScaling=False, topoScaling=False, outputDir=None):
    # os.system('pkill -9 ns3.36.1')

    if workload not in ["hadoop", "websearch", "alibaba", "microburst", "video"]:
        print(f"Unknown workload: {workload}")
        return os.EX_DATAERR

    if not outputDir:
        outputDir = get_default_output_dir(workload, topoScaling, gwScaling)

    if gwScaling or topoScaling:
        PERCENTAGES = [x / 100 for x in [50]]
    else:
        PERCENTAGES = [
            x / 100
            for x in [
                1,
                2,
                5,
                10,
                50,
                100,
                500,
                1000,
                5000,
                10000,
                50000,
                100000,
                150000,
            ]
        ]

    proc_handles = []
    next_experiment_num = get_next_experiment_num(outputDir)
    sim_modes = get_sim_modes()

    for p in PERCENTAGES:
        for mode in sim_modes:
            if mode in ['Direct', 'OnDemand', 'NoCache'] and p != PERCENTAGES[0]:
                continue
            if gwScaling or topoScaling:
                if mode not in ['SwitchV2P', 'NoCache', 'LocalLearning', 'GwCache']:
                    continue
            if 'Controller' in mode and not (workload == 'websearch' and p >= 0.1):
               continue
            configs = generate_configs(mode, workload, gwScaling, topoScaling)
            for config in configs:
                mem_size = int(p * ADDRESS_SPACE_SIZE[workload])
                config["p"] = p
                config["mem_size"] = mem_size
                config["simMode"] = mode
                experiment_dir = os.path.join(outputDir, str(next_experiment_num))
                next_experiment_num += 1
                os.makedirs(experiment_dir)
                output_file = os.path.join(experiment_dir, "results.json")

                cli = get_command_line(mode, workload, config, output_file, topoScaling, gwScaling)

                config["cli"] = cli
                with open(os.path.join(experiment_dir, "config.json"), "w") as out_file:
                    json.dump(config, out_file)
                command = ["time", os.path.join(NS3_HOME, "ns3"), "run", "--no-build", cli]
                print(
                    f"Running command = {command}. Output directory = {experiment_dir}"
                )
                stdout_file = open(os.path.join(experiment_dir, "out.txt"), "w")
                handle = subprocess.Popen(
                    command, cwd=NS3_HOME, stdout=stdout_file, stderr=stdout_file
                )
                proc_handles.append(handle)
                if len(proc_handles) >= MAX_PROC:
                    print(f"Reached {len(proc_handles)} running processes")
                    pid, _ = os.wait()
                    print(f"Process {pid} has finished")
                    finished_id = -1
                    for i in range(len(proc_handles)):
                        curr_proc = proc_handles[i]
                        if curr_proc.pid == pid:
                            finished_id = i

                    assert finished_id >= 0
                    proc_handles.pop(finished_id)
                    print(f"Popped process")

    for handle in proc_handles:
        handle.wait()

    return 0

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-w', dest='workload', type=str, required=True,
                    help='The workload to run')
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--gw', action=argparse.BooleanOptionalAction)
    group.add_argument('--topo', action=argparse.BooleanOptionalAction)
    args = parser.parse_args()

    run_workload(args.workload, args.gw, args.topo)

    return 0


if __name__ == "__main__":
    sys.exit(main())

