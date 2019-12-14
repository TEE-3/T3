#include "bitcoinapi/bitcoinapi.h"
#include "bitcoinapi/types.h"
#include <openssl/ripemd.h>
#include <openssl/sha.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string.h>
#include <bitset>
#include <stdio.h>

using namespace std;

struct txInputType{
    unsigned char txid[32];
    //double amount;
    //int blockheight;
    unsigned char pubkeyhash[20];
    int position;
};

struct txOutputType{
    unsigned char txid[32];
    double amount;
    int blockheight;
    unsigned char pubkeyhash[20];
    int position;
};

string username = "dduck";
string password = "12345678";
string address = "128.10.121.207";
int port = 8332;
BitcoinAPI btc(username, password, address, port);

blockinfo_t currentBlock;
FILE *inputFile, *outputFile;

bool StringToHex(const string &inStr, unsigned char *outStr)
{
    size_t len = inStr.length();
    for (size_t i = 0; i < len; i += 2) {
        sscanf(inStr.c_str() + i, "%2hhx", outStr);
        ++outStr;        
    }
    return true;
}

string sha256(const string str){
    unsigned char outStr[str.size()/2];
    StringToHex(str,outStr);
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, outStr, sizeof(outStr));
    SHA256_Final(hash, &sha256);
    stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++){
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

string ripemd160(const string str) {
    unsigned char outStr[str.size()/2];
    StringToHex(str,outStr);
    unsigned char hash[RIPEMD160_DIGEST_LENGTH];
    RIPEMD160_CTX ripemd160;
    RIPEMD160_Init(&ripemd160);
    RIPEMD160_Update(&ripemd160, outStr, sizeof(outStr));
    RIPEMD160_Final(hash, &ripemd160);
    stringstream ss;
    for(int i = 0; i < RIPEMD160_DIGEST_LENGTH; i++){
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

int getTxInput(decoderawtransaction_t rawtx){

    int numVins = rawtx.vin.size(), r;
    txInputType *txinput;

    //Iterate over each input of the transaction
    for(int k=0; k<numVins; k++){
        
        // We are only interested in P2PKH transactions, so we assume that every hex representation
        // of the scriptSig which length is equal to 214, corresponds to the size of a signature+pubkey
        // carateristic of a scriptSig that solves the scriptPubkey of a P2PKH transaction.
        if(rawtx.vin[k].scriptSig.hex.length()==214){
            
            //Allocate memory for txinput
            txinput = (txInputType*) malloc(sizeof(txInputType));

            //Tx hash
            StringToHex(rawtx.vin[k].txid, txinput->txid);

            //Public key hash                        
            StringToHex(ripemd160(sha256(rawtx.vin[k].scriptSig.hex.substr(148))), txinput->pubkeyhash);

            //Position
            txinput->position = k;

            //Save txinput struct in inputFile
            r = fwrite(txinput , sizeof(txInputType) , 1 , inputFile);
            if(r == 0){ 
                perror("Error fail to fwrite txinput into inputFile"); 
                exit(-1); 
            }
            free(txinput);
        }                         
    }
    return 0;
}

int getTxOutput(decoderawtransaction_t rawtx){
    
    int numVouts = rawtx.vout.size(), r;
    txOutputType *txoutput;
    
    //Iterate over each output of the transaction
    for(int k=0; k<numVouts; k++){
        
        //We are only interested in P2PKH transactions
        if(rawtx.vout[k].scriptPubKey.type=="pubkeyhash"){

            //Allocate memory for txoutput
            txoutput = (txOutputType*) malloc(sizeof(txOutputType));
            
            //Tx hash                     
            StringToHex(rawtx.txid, txoutput->txid);
            cout << "txid " << rawtx.txid << endl; 
            
            //Amount
            txoutput->amount = rawtx.vout[k].value;
            cout << "amount " << txoutput->amount << endl;           

            //Block height
            txoutput->blockheight = currentBlock.height;

            //Public key hash
            StringToHex(rawtx.vout[k].scriptPubKey.hex.substr(6, 40), txoutput->pubkeyhash);

            //Position
            txoutput->position = k;
            
            //Save txinput struct in inputFile
            r = fwrite(txoutput , sizeof(txOutputType) , 1 , outputFile);
            if(r == 0){ 
                perror("Error fail to fwrite txoutput into outputFile"); 
                exit(-1); 
            }
            free(txoutput);

        }
    }
    return 0;
}

//Transactions in the current block
int getTransactionsInfo(const int numTxs){

    //Iterate over each transaction in the block
    for(int j=0; j<numTxs; j++){

        //Get transaction[j] details
        decoderawtransaction_t rawtx = btc.getrawtransaction(currentBlock.tx[j],1);
        getTxInput(rawtx);
        getTxOutput(rawtx);                
    }    
    return 0;  
}


int main()
{

    try
    {        
        int blockcount= 0; 
        int i = 510299;
         
        blockcount = btc.getblockcount();

        
        inputFile = fopen("input.bin", "wb");
        if(inputFile == NULL){
            perror("Error creating the file: ");
            exit(-1);
        }
        outputFile = fopen("output.bin", "wb");
        if(outputFile == NULL){
            perror("Error creating the file: ");
            exit(-1);
        }

        cout << "txInputType\t" << sizeof(txInputType) << endl;
        cout << "txOutputType\t" << sizeof(txOutputType) << endl;

        while(i < 510300)
        //while(i < blockcount)
        {
            //General info about the block
            string blockHash = btc.getblockhash(i);
            currentBlock = btc.getblock(blockHash);
            getTransactionsInfo(currentBlock.tx.size());
            i++;
        }

        fclose(inputFile);
        fclose(outputFile);
        
    }
    catch(BitcoinException e)
    {
        cerr << e.getMessage() << endl;
    }

    return 0;
}
