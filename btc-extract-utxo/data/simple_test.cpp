#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
//void sha256(unsigned char* pkh, unsigned char* sha)
//{
//    SHA256_CTX sha256;
//    SHA256_Init(&sha256);
//    SHA256_Update(&sha256, pkh, sizeof(*pkh));
//    SHA256_Final(sha,&sha256);
//      sha = SHA256(pkh, 20,0);
//}
void hexdump(unsigned char *a, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++)
      printf("%02x", a[i]);
    printf("\n");
}

int main () {
    FILE *utxo_fd;
    utxo_fd = fopen("biggerUTXO", "r+");
    unsigned char *data_in;
    data_in = (unsigned char*) malloc(68);
    int i  = 0;
    while((fread(data_in, 68, 1, utxo_fd)!=0) && (i < 96))
    {
        unsigned char *pkh = (unsigned char*) malloc(20);
        memcpy(pkh, &data_in[44],20);
        hexdump(pkh,20);
        unsigned char *hash256 = (unsigned char*) malloc(32);
        //sha256(pkh,hash256);
        //hash256 = SHA256(pkh,20,0);
        SHA256(pkh,20,hash256);
        uint32_t num;
        hexdump(hash256,32);
        num = (hash256[0] << 24) | (hash256[1] << 16) | (hash256[2] << 8) | (hash256[3]);
        printf("%d\n", num%2097152);
        i++;
    }
    return 0;
}
