# T3 Project
<!-- ----------------- -->

- ```btc-module``` contains the code used to generate the numbers reported in the T3 paper.

- ```btc-extract-utxo``` is python code to read data from **chainstate** folder and extract to raw utxo.
- ```btc-reducer``` is C++ implementation to talk to **bitcoind** to obtain bitcoin transactions and blocks. 
- ```scripts``` contains scripts to build sgx-sdk. 
- ```sgx-bitcoin``` is the non-recursive implementation of path oram

## Pre-requisites:

T3 requires Intel SGX_SDK stack, we tested it with SGX SDK 2.1 release. To set up SGX-SDK please refer to: [https://github.com/intel/linux-sgx](https://github.com/intel/linux-sgx)

Also, T3 is built on top: 
- Zerotrace. Please refer to [https://github.com/sshsshy/ZeroTrace](https://github.com/sshsshy/ZeroTrace) for detailed documentations.

## Execution
The numbers reported in the paper are obtained by compiling the ```bitcoin-module``` folder. 
The extract numbers requires a system that has at least 64gb of RAM, and a bitcoin node running. 

For testing purpose in your system, please modify the ```oram-test.sh``` and  ```exec.sh```, so that the code can be executed in your system.

## Note
This implementation is just for the proof of concentp. ere are several places in the code that were implemented INSECURELY for the sake of code readibility and understanding. We are not responsible for any damages if the code is used for commercial purposes.

## Further Contact
For any inquiries. please contact me (le52@purdue.edu)