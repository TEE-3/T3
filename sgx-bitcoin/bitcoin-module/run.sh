#!/bin/bash -e

if [ -d "../linux-sgx" ]; then

DEBUG=0

echo "Number of args $#"

if [ "$#" -ne 2 ]; then
  echo "Usage: ./run.sh [DEBUG] [BLOCK_SIZE]"
  exit
fi

if [ "$1" == "1" ]; then
  echo "Debugging ON"
  DEBUG=1
else
  echo "Debugging OFF"
  DEBUG=0
fi

echo "Custom Block Size: $2"

source ../linux-sgx/linux/installer/bin/sgxsdk/environment

make clean

make CUSTOM_DEBUG=$DEBUG CUSTOM_BLOCK_SIZE=$2 > log.make 2>&1

./app

else

  echo "[X] Please install the Linux SGX SDK using ../script/install-sgx-sdk.sh"

fi