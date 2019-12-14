#include<iostream>
#include<sstream>

struct txInputType{
    unsigned char txid[32];         //32 bytes
    uint64_t amount;                //8  bytes
    uint32_t blockheight;           //4  bytes
    unsigned char pubkeyhash[20];   //20 bytes
    uint32_t position;              //4  bytes
};

struct txOutputType{
    unsigned char txid[32];        
    uint64_t amount;
    uint32_t blockheight;
    unsigned char pubkeyhash[20];
    uint32_t position;
};
