
# T3 Project

<!-- ----------------- -->

-  ```btc-module``` contains the code used to generate the numbers reported in the T3 paper.
-  ```btc-extract-utxo``` uses [Bitcoin_tools](https://github.com/sr-gi/bitcoin_tools) extract the Bitcoin blockchain data to obtain the recent utxo. This allows T3 to obtain the UTXO set to build ORAM tree without manually parsing the bitcoin data.
-  ```scripts``` contains scripts to set up dependencies for T3.
<!-- -  ```btc-reducer``` contains the to talk to **bitcoind** to obtain bitcoin transactions and blocks. -->
<!-- -  ```sgx-bitcoin``` contains the non-recursive implementation of path ORAM.  -->

  

## Pre-requisites:

* T3 was tested on 64-bit Ubuntu 18.04  
* T3 requires Intel SGX_SDK stack, we tested it with SGX SDK 2.1.3 release. To set up SGX-SDK please refer to: [Intel SGX](https://github.com/intel/linux-sgx)
* T3 is built on top of Zerotrace. Please refer to [Zerotrace](https://github.com/sshsshy/ZeroTrace) for detailed documentations.


## Execution

The numbers reported in the paper are obtained by compiling the ```bitcoin-module``` folder.
The extract numbers requires a system that has at least 64gb of RAM, and a bitcoin node running.

However, for testing purpose in your system, please try the following step:

Installing dependencies: 

    cd scripts/
    ./install.sh
    
To obtain the testing result, enter bitcoin-module: 

    cd btc-module/
    source <sdk folder>/enviroment
    ./bitcoin-test opt

Other Notes:

* Depends on the system memory, edit the exec.sh accordingly.
In particular, parameter **N**, **Z**, and **block_size** determine how big the ORAM tree will be allocated in the memory. 

* T3 uses [bitcoin-api-cpp](https://github.com/minium/bitcoin-api-cpp) to handle the RPC communication with **bitcoind**. This allows T3 to obtain + verify new bitcoin blocks. But the current code is potentially broken and does not work blocks with segwit transactions.  

* Running ./bitcointest with `opt` flag (optimized) returns the running time of 1000 **read_only** oram accesses and running time of 1000 standard access. Running ./bitcointest with `simple` flag return running time of **standard** ORAM access. 

## Docker
To test with docker, run:

    docker pull diamondduck/tee-3-image:helloworld
    docker run -ti diamondduck/tee-3-image:helloworld
    cd
    cd T3/btc-module
    source /opt/intel/sgxsdk/environment
    ./bitcoin-test.sh opt

## Note

This implementation is just the proof of concept. There are several places in the code that were implemented INSECURELY for the sake of code readibility and understanding. We are not responsible for any damages if the code is used for commercial purposes.

  

## Further Contact

For any inquiries. please contact me (le52@purdue.edu)
