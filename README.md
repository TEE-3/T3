# SGX Bitcoin Project
<!-- ----------------- -->
- ```btc-extract-utxo``` is python code to read data from **chainstate** folder and extract to raw utxo.
- ```btc-reducer``` is C++ implementation to talk to **bitcoind** to obtain bitcoin transactions and blocks. We still need to implement the pruning and verifying bitcoin block. 
- ```ZeroTrace``` is the recursive implementation of **path** and **circuit**-ORAM. At this point, it can initialize the ORAM tree using the data output by ```btc-extract-utxo```. After the initilization, we want this code to update the oram tree using data output by ```btc-reducer```.
- ```sgx-bitcoin``` is the non-recursive implementation of path oram
