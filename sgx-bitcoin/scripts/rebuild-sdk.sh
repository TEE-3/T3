#!/bin/bash -e

if [ -d "../linux-sgx" ]; then

version="2.0.40950"

pushd ../linux-sgx/

  echo "yes" | linux/installer/bin/sgx_linux_x64_sdk_${version}.bin

  if [ -d "/opt/intel/sgxpsw" ]; then
    pushd /opt/intel/sgxpsw
    sudo ./uninstall.sh
    popd
  fi

  pushd psw
    make
  popd


  make psw_install_pkg

  sudo linux/installer/bin/sgx_linux_x64_psw_${version}.bin

  pushd psw/urts/linux/
    cp libsgx_urts.so ../../../sgxsdk/lib64/
  popd

popd

echo "[*] Rebuild was successful"

else
  echo "[X] Please install Linux SGX SDK by running install-sgx-sdk.sh"
fi
