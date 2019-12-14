#!/bin/bash -e

pushd ../

rm -rf linux-sgx

version="2.0.40950"

# Pre-reqs for SGX SDK
sudo apt-get install build-essential ocaml automake autoconf libtool wget python

# Pre-reqs for SGX PSW
sudo apt-get install libssl-dev libcurl4-openssl-dev protobuf-compiler libprotobuf-dev

# Clone the SGX SDK repo
git clone https://github.com/01org/linux-sgx.git

pushd linux-sgx

  # Starting commit used
  git checkout cbdce63643e84088749794f5044a9916af2b19ce

  make clean

  ./download_prebuilt.sh

  make

  make sdk_install_pkg

  make psw_install_pkg

  pushd linux/installer/bin
    echo "yes" | ./sgx_linux_x64_sdk_${version}.bin
    sudo bash /opt/intel/sgxpsw/uninstall.sh || true
    sudo ./sgx_linux_x64_psw_${version}.bin
  popd
popd

popd