#!/bin/bash -e

if [ "$1" == "clean" ];
then
  make clean
  make SGX_MODE=SIM ORAM_TESTING=1 OPTIMIZED=1
elif [ "$1" == "simple" ];
then
  make clean
  make SGX_MODE=SIM ORAM_TESTING=1
fi

./exec.sh