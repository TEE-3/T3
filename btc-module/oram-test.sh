#!/bin/bash -e

if [ "$1" == "clean" ];
then
  make clean
fi
make ORAM_TESTING=1
./exec.sh