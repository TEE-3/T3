#include "../Globals_Enclave.hpp"
#include "../crypto/ripemd160.h"

struct txInOut68_t{
    unsigned char u68bytes[68];
};

struct blockHeader
{   
    uint8_t nVersion[4];
    uint8_t hashPreviousBlock[32];
    uint8_t merkleRoot[32];
    uint8_t nTime[4];
    uint8_t nBit[4];
    uint8_t nNonce[4];
};

struct tx_t{
    sgx_sha256_hash_t txhash;
    unsigned char * hex;
    uint32_t hexSize; 
};

void doubleSHA256(uint8_t* data, uint32_t len, sgx_sha256_hash_t *output);
void ripemd160(uint8_t * data, uint32_t len, uint8_t hash[20]);
size_t getRealSizeFromCompactSize(unsigned char * hex, size_t offset, size_t * realSize);
size_t getSegWitTxInputs(tx_t * tx, size_t offset, size_t vin_size);
size_t getTxInputs(tx_t * tx, size_t offset);
size_t getTxOutputs(tx_t * tx, size_t offset, uint32_t blockheight);
size_t getSegWitP2PKH(tx_t * tx, size_t offset, size_t vin_size);
int reducer(tx_t * listTxs, size_t numTxs, uint32_t blockheight, std::vector<txInOut68_t> * srt_vin, std::vector<txInOut68_t> * srt_vout);
