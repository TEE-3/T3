#!/bin/bash -e

pushd ../

# Clone the SGX driver repo
git clone https://github.com/01org/linux-sgx-driver.git || true

pushd linux-sgx-driver

  # Starting commit used
  git checkout 03435d33de0bcca6c5777f23ac161249b9158f1e

  make clean
  make
  sudo mkdir -p "/lib/modules/"`uname -r`"/kernel/drivers/intel/sgx"    
  sudo cp isgx.ko "/lib/modules/"`uname -r`"/kernel/drivers/intel/sgx"    
  sudo sh -c "cat /etc/modules | grep -Fxq isgx || echo isgx >> /etc/modules"    
  sudo /sbin/depmod
  sudo /sbin/modprobe isgx

popd

popd