#include "reducer.hpp"

std::vector<txInOut68_t> * vin;
std::vector<txInOut68_t> * vout;
size_t txInputs = 0;
size_t txOutputs = 0;
size_t skippedIn = 0;
size_t skippedOut = 0;

void doubleSHA256(uint8_t* data, uint32_t len, sgx_sha256_hash_t *output)
{
    sgx_sha256_hash_t tempOutput1; 
    sgx_sha256_msg(data, len, &tempOutput1);
    sgx_sha256_msg(tempOutput1, SGX_SHA256_HASH_SIZE, output); // 
}

void ripemd160(uint8_t * data, uint32_t len, uint8_t hash[20])
{ 
    CRIPEMD160().Write(data,SGX_SHA256_HASH_SIZE).Finalize(hash);
}

size_t getRealSizeFromCompactSize(unsigned char * hex, size_t offset, size_t * realSize)
{

    *realSize = 0;
    if(hex[offset]<=0xfc){                
        *realSize = hex[offset];
        offset+=1;
    }else if(hex[offset]==0xfd){
        offset+=1;
        memcpy(realSize, &hex[offset], 2);
        offset+=2;
    }else if(hex[offset]==0xfe){
        offset+=1;
        memcpy(realSize, &hex[offset], 4);
        offset+=4;
    }else if(hex[offset]==0xff){
        offset+=1;
        memcpy(realSize, &hex[offset], 8);
        offset+=8;
    }

    return offset;
}

size_t getTxInputs(tx_t * tx, size_t offset, size_t vin_size)
{   
    for(size_t j=0; j<vin_size; j++)
    {
        txInOut68_t tin;
        
        // prev Tx hash
        memcpy(&tin.u68bytes[0], &tx->hex[offset], 32);
        offset+=32;

        // fill amount and blockheight with zeros
        memset(&tin.u68bytes[32], 0, 12);

        // prev Tx output position
        memcpy(&tin.u68bytes[64], &tx->hex[offset], 4);
        offset+=4;

        

        if(tx->hex[offset]!=0x00){
        
            // Extract script bytes size from compact size format
            size_t script_bytes;
            offset = getRealSizeFromCompactSize(tx->hex, offset, &script_bytes);
            size_t in_offset = offset;
            //printf("script_bytes: %d, %zx\n", script_bytes, script_bytes);
            
            
            // We are only interested in P2PKH transactions, so we assume that every hex representation
            // of the scriptSig which length in bytes is equal to 214 (107 bytes), corresponds to the size of a signature+pubkey
            // carateristic of a scriptSig that solves the scriptPubkey of a P2PKH transaction.
            
            // script_bytes = (1 + sig_bytes + 1) + pk
            // pk = 65 or 33 (uncompressed or compressed format respectively)
            
            uint8_t sig_bytes;
            memcpy(&sig_bytes, &tx->hex[in_offset], 1);
            in_offset+=2+sig_bytes;

            //printf("script_bytes: %zu, sig_bytes: %d, sum1: %d, sum2: %d\n", script_bytes, sig_bytes, sig_bytes + 67, sig_bytes + 35);
        
        
            if(script_bytes==sig_bytes + 35){ // Compressed pk of 33 bytes
                
                // SHA256 
                sgx_sha256_hash_t tempOutput;  
                sgx_sha256_msg(&tx->hex[in_offset], 33, &tempOutput);
                
                // RIPEMD160
                ripemd160(&tempOutput[0], 32, &tin.u68bytes[44]);

                (*vin).push_back(tin);
            
            }else if(script_bytes==sig_bytes + 67){ // Uncompressed pk of 65 bytes

                // SHA256 
                sgx_sha256_hash_t tempOutput; 
                sgx_sha256_msg(&tx->hex[in_offset], 65, &tempOutput);
                
                // RIPEMD160
                ripemd160(&tempOutput[0], 32, &tin.u68bytes[44]);

                (*vin).push_back(tin);
            }else{
                skippedIn++;
            } 
            offset += script_bytes;   
        }else{
            offset++;
        } 
        offset += 4; //sequence      
       
    }
    return offset;
}

size_t getTxOutputs(tx_t * tx, size_t offset, uint32_t blockheight)
{
    //Extract output vector's size from compact size format
    size_t vout_size;
    offset = getRealSizeFromCompactSize(tx->hex, offset, &vout_size);
    // printf("# Tx outputs: %d\n", vout_size);
    txOutputs += vout_size;

    for(size_t k=0; k<vout_size; k++){
        txInOut68_t tout;
        
        // amount
        memcpy(&tout.u68bytes[32], &tx->hex[offset], 8);
        // printf("%llu\n", tout.amount);
        offset+=8;

        // We are only interested in P2PKH transactions, so we look in every
        // output if the pubkey script size is of 25 (0x19) bytes and if the first
        // byte of the script is OP_DUP (0x76), characteristic of P2PKH transactions
        if(tx->hex[offset] == 0x19 && tx->hex[offset+1] == 0x76){
            
            // pk hash
            memcpy(&tout.u68bytes[44], &tx->hex[offset+4], 20);
            
            // txhash
            memcpy(&tout.u68bytes[0], &tx->txhash, 32);

            //position
            memcpy(&tout.u68bytes[64], &k, 4);

            //blocknumber
            memcpy(&tout.u68bytes[40], &blockheight, 4);

            (*vout).push_back(tout);
        }else{
            skippedOut++;
        }
        
        offset+=1+tx->hex[offset];
        
    }

    
    return offset;
}

int reducer(tx_t * listTxs, size_t numTxs, uint32_t blockheight, std::vector<txInOut68_t> * srt_vin, std::vector<txInOut68_t> * srt_vout)
{
    vin = srt_vin;
    vout = srt_vout;

    //TODO: Deal with the coinbase tx
    //getCoinbase();
    int segWit=0;

    for(size_t i=1; i<numTxs; i++){
        // printf("Tx %d hex: \n", i);
        // hexdump(listTxs[i].hex, listTxs[i].hexSize);
        // printf("\n");
        size_t offset; // Current index in tx_t hex
        offset = 4; // nVersion
        bool isSegWit = false;

        // SegWit Extended transaction serialization format
        if(listTxs[i].hex[offset]==0x00 && listTxs[i].hex[offset+1]!=0x00){
            offset+=2;
            isSegWit = true;
            segWit++;            
        }

        // Extract input vector's size from compact size format
        size_t vin_size;
        offset = getRealSizeFromCompactSize(listTxs[i].hex, offset, &vin_size);
        txInputs += vin_size;
        // printf("# Tx inputs: %d\n", vin_size);
        offset = getTxInputs(&listTxs[i], offset, vin_size);

        offset = getTxOutputs(&listTxs[i], offset, blockheight);

        // Segregated witness tx?
        // if(isSegWit)
        // {
        //     for(size_t k=0; k<vin_size; k++)
        //     {
                
        //     }
        // }
    
        
        // printf("\n----------------------------------\n");
    }
    printf("vin size\t %d\n", txInputs);
    printf("pruned vin  \t %d\n", (*vin).size());
    printf("skippedIn\t %d\n", skippedIn);
    printf("segWit\t\t %d\n\n", segWit);

    printf("vout size\t %d\n", txOutputs);    
    printf("pruned vout \t %d\n", (*vout).size());
    printf("skippedOut\t %d\n\n", skippedOut);
        
    
    skippedIn = 0;
    skippedOut = 0;
    
    return 0;
}