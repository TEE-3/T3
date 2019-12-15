
# T3 Project

<!-- ----------------- -->

-  ```btc-module``` contains the code used to generate the numbers reported in the T3 paper.

-  ```btc-extract-utxo``` is python code to read data from **chainstate** folder and extract to raw utxo.

-  ```btc-reducer``` is C++ implementation to talk to **bitcoind** to obtain bitcoin transactions and blocks.

-  ```scripts``` contains scripts to set up dependencies for t3.

-  ```sgx-bitcoin``` is the non-recursive implementation of path oram

  

## Pre-requisites:

  

T3 requires Intel SGX_SDK stack, we tested it with SGX SDK 2.1.3 release. To set up SGX-SDK please refer to: [https://github.com/intel/linux-sgx](https://github.com/intel/linux-sgx)

  

Also, T3 is built on top:

- Zerotrace. Please refer to [https://github.com/sshsshy/ZeroTrace](https://github.com/sshsshy/ZeroTrace) for detailed documentations.

  

## Execution

The numbers reported in the paper are obtained by compiling the ```bitcoin-module``` folder.
The extract numbers requires a system that has at least 64gb of RAM, and a bitcoin node running.

For testing purpose in your system, please try the following step:
Installing dependencies: 

    cd scripts/
    ./install.sh
    
To obtain the testing result, enter bitcoin-module: 

    cd btc-module/
    source <sdk folder>/enviroment
    ./bitcoin-test opt

depends on the spec of your system, edit the exec.sh accordingly.
  

## Note

This implementation is just for the proof of concept. There are several places in the code that were implemented INSECURELY for the sake of code readibility and understanding. We are not responsible for any damages if the code is used for commercial purposes.

  

## Further Contact

For any inquiries. please contact me (le52@purdue.edu)
