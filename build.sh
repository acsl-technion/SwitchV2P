#!/bin/bash

my_dir=$(readlink -f $PWD)
ns3_dir=$my_dir/ns3

echo "Installing dependencies..."
sudo apt install libz3-dev libboost-dev

if [[ ! -e $ns3_dir ]]; then
    echo "Cloning NS3 3.36.1 into $ns3_dir..."
    git clone --depth 1 --branch ns-3.36.1 https://github.com/nsnam/ns-3-dev-git $ns3_dir
fi

if [[ ! -d $ns3_dir/scratch/switchv2p ]]; then
    echo "Copying SwitchV2P..."
    (cd switchv2p/datasets && gunzip *)
    cp -r switchv2p $ns3_dir/scratch
fi

echo "Patching NS3..."
(cd $ns3_dir && git apply ../ns3-3.36.1.patch)

echo "Building NS3..."
(cd $ns3_dir && 
 ./ns3 configure -d optimized --enable-modules core,network,point-to-point,virtual-net-device,applications,internet,csma &&
 ./ns3 build)
