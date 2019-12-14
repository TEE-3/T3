#!/bin/bash -e
pushd ../
    git clone https://github.com/minium/bitcoin-api-cpp.git

pushd bitcoin-api-cpp
    sudo apt-get install cmake libcurl4-openssl-dev libjsoncpp-dev
    mkdir build   
pushd build
    cmake ..
    make
    sudo make install
    sudo ldconfig
popd
popd
    rm -rf bitcoin-api-cpp
popd

