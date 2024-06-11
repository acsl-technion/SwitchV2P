#!/bin/bash

cwd=$(readlink -f $PWD)
export NS3_HOME=$cwd/ns3
