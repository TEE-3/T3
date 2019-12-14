/*
*    ZeroTrace: Oblivious Memory Primitives from Intel SGX 
*    Copyright (C) 2018  Sajin (sshsshy)
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, version 3 of the License.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <vector>
#include "Globals_Enclave.hpp"
#include "ORAMTree.hpp"
#include "PathORAM_Enclave.hpp"
#include "CircuitORAM_Enclave.hpp"
#include "reducer/reducer.hpp"

std::vector<PathORAM *> poram_instances;
std::vector<CircuitORAM *> coram_instances;

uint32_t poram_instance_id=0;
uint32_t coram_instance_id=0;

uint32_t createNewORAMInstance(uint32_t max_blocks, uint32_t data_size, uint32_t stash_size, uint32_t oblivious_flag, uint32_t recursion_data_size, int8_t recursion_levels, uint64_t onchip_posmap_mem_limit, uint32_t oram_type, uint8_t pZ){

	if(oram_type==0){
		PathORAM *new_poram_instance = (PathORAM*) malloc(sizeof(PathORAM));
		poram_instances.push_back(new_poram_instance);
		
		#ifdef DEBUG_ZT_ENCLAVE
      printf("Creating Path ORAM Instance\n");
      printf("Block Size: %ld\n", data_size);
			printf("Recursion Levels = %d\n", recursion_levels);	
		#endif
		
		new_poram_instance->Initialize(pZ, max_blocks, data_size, stash_size, oblivious_flag, recursion_data_size,
              recursion_levels, onchip_posmap_mem_limit);
		
		// [Lizzy]
		// Initialize ORAM bloks with zeros
		// unsigned char data_in[data_size], data_out[data_size]; 
		// memset(&data_in, 0, data_size);
		// for(int i=0; i<max_blocks; i++){
		// 	new_poram_instance->Access_temp(i, 'w', &data_in[0], &data_out[0]);
		// }
		
		return poram_instance_id++;
	}
	else if(oram_type==1){
		CircuitORAM *new_coram_instance = (CircuitORAM*) malloc(sizeof(CircuitORAM));
		coram_instances.push_back(new_coram_instance);

		#ifdef DEBUG_ZT_ENCLAVE
      printf("Creating Circuit ORAM Instance\n");
      printf("Block Size: %ld\n", data_size);
			printf("Recursion Levels = %d\n", recursion_levels);	
		#endif

		new_coram_instance->Initialize(pZ, max_blocks, data_size, stash_size, oblivious_flag, recursion_data_size,
              recursion_levels, onchip_posmap_mem_limit);

		// [Lizzy]
		// Initialize ORAM bloks with zeros
		// unsigned char data_in[data_size], data_out[data_size]; 
		// memset(&data_in, 0, data_size);
		// for(int i=0; i<max_blocks; i++){
		// 	new_coram_instance->Access_temp(i, 'w', &data_in[0], &data_out[0]);
		// }

		return coram_instance_id++;
	}
}


void accessInterface(uint32_t instance_id, uint8_t oram_type, unsigned char *encrypted_request, unsigned char *encrypted_response, unsigned char *tag_in, unsigned char* tag_out, uint32_t encrypted_request_size, uint32_t response_size, uint32_t tag_size){

	//TODO : Would be nice to remove this dynamic allocation.
	PathORAM *poram_current_instance;
	CircuitORAM *coram_current_instance;

	unsigned char *data_in, *data_out, *request, *request_ptr;
	uint32_t id, opType;
	uint8_t pkh[20];
	request = (unsigned char *) malloc (encrypted_request_size);
	data_out = (unsigned char *) malloc (response_size);	

	sgx_status_t status = SGX_SUCCESS;
	status = sgx_rijndael128GCM_decrypt((const sgx_aes_gcm_128bit_key_t *) SHARED_AES_KEY, (const uint8_t *) encrypted_request,
                                    encrypted_request_size, (uint8_t *) request, (const uint8_t *) HARDCODED_IV, IV_LENGTH,
                                    NULL, 0, (const sgx_aes_gcm_128bit_tag_t*) tag_in);
	/*	
	if(status == SGX_SUCCESS)
		printf("Decrypt returned Success flag\n");
	else{
		if(status == SGX_ERROR_INVALID_PARAMETER)
			printf("Decrypt returned SGX_ERROR_INVALID_PARAMETER Failure flag\n");		
		else
			printf("Decrypt returned another Failure flag\n");
	}
	*/

	//Extract Request Id and OpType
	opType = request[0];
	request_ptr = request+1;
	memcpy(&pkh, request_ptr, PKH_SIZE_IN_BYTES); 
	
	//TODO: error checking
	sgx_sha256_hash_t hash;
    sgx_sha256_msg((uint8_t*)&pkh, PKH_SIZE_IN_BYTES, &hash);
    uint32_t num = (hash[0] << 24) | (hash[1] << 16) | (hash[2] << 8) | (hash[3]);
	for( int k = 0; k < 20; k++ ){
		if( k % 16 == 0 ) printf("\n");
		printf("%2x ", (unsigned)pkh[k]);
	}
	id = num % 2097152; //2^n
	printf("\nid: %u\n", id);

	//printf("Request Type = %c, Request_id = %d", opType, id);
	data_in = request_ptr+PKH_SIZE_IN_BYTES;

	if(oram_type==0){
		poram_current_instance = poram_instances[instance_id];
		poram_current_instance->Access_temp(id, opType, data_in, data_out);
	}
	else {
		coram_current_instance = coram_instances[instance_id];
		coram_current_instance->Access_temp(id, opType, data_in, data_out);
	}

	//Encrypt Response
	status = sgx_rijndael128GCM_encrypt((const sgx_aes_gcm_128bit_key_t *) SHARED_AES_KEY, data_out, response_size,
                                        (uint8_t *) encrypted_response, (const uint8_t *) HARDCODED_IV, IV_LENGTH, NULL, 0,
                                        (sgx_aes_gcm_128bit_tag_t *) tag_out);
	/*
	if(status == SGX_SUCCESS)
		printf("Encrypt returned Success flag\n");
	else{
		if(status == SGX_ERROR_INVALID_PARAMETER)
			printf("Encrypt returned SGX_ERROR_INVALID_PARAMETER Failure flag\n");		
		else
			printf("Encrypt returned another Failure flag\n");
	}	
	*/

	free(request);
	free(data_out);

}

void accessBulkReadInterface(uint32_t instance_id, uint8_t oram_type, uint32_t no_of_requests, unsigned char *encrypted_request, unsigned char *encrypted_response, unsigned char *tag_in, unsigned char* tag_out, uint32_t encrypted_request_size, uint32_t response_size, uint32_t tag_size){
	//TODO : Would be nice to remove this dynamic allocation.
	PathORAM *poram_current_instance;
	CircuitORAM *coram_current_instance;
	unsigned char *data_in, *request, *request_ptr, *response, *response_ptr;
	uint32_t id;
	char opType = 'r';

	uint32_t tdata_size;
	if(oram_type==0){
		poram_current_instance = poram_instances[instance_id];
		tdata_size = poram_current_instance->data_size;
	}	
	else{
		coram_current_instance = coram_instances[instance_id];
		tdata_size = coram_current_instance->data_size;
	}
	
	request = (unsigned char *) malloc (encrypted_request_size);
	response = (unsigned char *) malloc (response_size);	
	data_in = (unsigned char *) malloc(tdata_size);

	sgx_status_t status = SGX_SUCCESS;
	status = sgx_rijndael128GCM_decrypt((const sgx_aes_gcm_128bit_key_t *) SHARED_AES_KEY, (const uint8_t *) encrypted_request,
                                    encrypted_request_size, (uint8_t *) request, (const uint8_t *) HARDCODED_IV, IV_LENGTH,
                                    NULL, 0, (const sgx_aes_gcm_128bit_tag_t*) tag_in);
	/*
	if(status == SGX_SUCCESS)
		printf("Decrypt returned Success flag\n");
	else{
		if(status == SGX_ERROR_INVALID_PARAMETER)
			printf("Decrypt returned SGX_ERROR_INVALID_PARAMETER Failure flag\n");		
		else
			printf("Decrypt returned another Failure flag\n");
	}
	*/

	request_ptr = request;
	response_ptr = response;

	for(int l=0; l<no_of_requests; l++){			
		//Extract Request Ids
		memcpy(&id, request_ptr, ID_SIZE_IN_BYTES);
		request_ptr+=ID_SIZE_IN_BYTES; 

		//TODO: Fix Instances issue.
		if(oram_type==0)
			poram_current_instance->Access_temp(id, opType, data_in, response_ptr);
		else
			coram_current_instance->Access_temp(id, opType, data_in, response_ptr);
		response_ptr+=(tdata_size);
	}

	//Encrypt Response
	status = sgx_rijndael128GCM_encrypt((const sgx_aes_gcm_128bit_key_t *) SHARED_AES_KEY, response, response_size,
                                        (uint8_t *) encrypted_response, (const uint8_t *) HARDCODED_IV, IV_LENGTH, NULL, 0,
                                        (sgx_aes_gcm_128bit_tag_t *) tag_out);

	/*
	if(status == SGX_SUCCESS)
		printf("Encrypt returned Success flag\n");
	else{
		if(status == SGX_ERROR_INVALID_PARAMETER)
			printf("Encrypt returned SGX_ERROR_INVALID_PARAMETER Failure flag\n");		
		else
			printf("Encrypt returned another Failure flag\n");
	}
	*/

	free(request);
	free(response);
	free(data_in);

}

int verifyMerkleRoot(uint8_t* txHashes, size_t len, uint8_t* merkleRoot)
{
    int32_t transactionNums = len/32;
    // if there is only one transaction hash, then it's the merkle root
    if(transactionNums == 1)
    {
        if(memcmp(txHashes, merkleRoot, SGX_SHA256_HASH_SIZE) == 0)
        {
            printf("[+] Computed Merkle Root match !!!\n");
            printf("[+] Computed Root:\n");
            hexdump(txHashes,SGX_SHA256_HASH_SIZE);
            printf("[e] Given Root:\n");
            hexdump(merkleRoot,SGX_SHA256_HASH_SIZE);
            return 0;
        }
        else
        {
            printf("[-] Merkle Root does not match\n");
            printf("[-] Computed Root:\n");
            hexdump(txHashes,SGX_SHA256_HASH_SIZE);
            printf("[e] Given Root:\n");
            hexdump(merkleRoot,SGX_SHA256_HASH_SIZE);
            return -1;
        }
    }
    // if it's even
    if(transactionNums%2 == 0)
    {
        int i;
        for (i = 0; i < len; i+=64)
        {
            sgx_sha256_hash_t output; 
            doubleSHA256(&txHashes[i], 2*SGX_SHA256_HASH_SIZE, &output);
            memcpy(&txHashes[i/2], output, SGX_SHA256_HASH_SIZE);
        }
        len = 32*transactionNums/2;
        return verifyMerkleRoot(txHashes, len, merkleRoot);
    }
    else
    {
        int i;
        for (i = 0; i < len; i+=64)
        {
            /* code */
            sgx_sha256_hash_t output; 
            doubleSHA256(&txHashes[i], 2*SGX_SHA256_HASH_SIZE, &output);
            memcpy(&txHashes[i/2], output, SGX_SHA256_HASH_SIZE);
        }
        /* code */
        i = len-32;
        uint8_t *duplicate =  (uint8_t*) malloc(2*SGX_SHA256_HASH_SIZE*sizeof(uint8_t));
        memcpy(&duplicate[0],  &txHashes[i], SGX_SHA256_HASH_SIZE);
        memcpy(&duplicate[32], &txHashes[i], SGX_SHA256_HASH_SIZE);
        sgx_sha256_hash_t output; 
        doubleSHA256(duplicate, 2*SGX_SHA256_HASH_SIZE, &output);
        len = 32*(transactionNums/2+1);
        memcpy(&txHashes[len-32], output, SGX_SHA256_HASH_SIZE);
        return verifyMerkleRoot(txHashes, len, merkleRoot);
    }
}

int verifyBlock(unsigned char * blockheader, tx_t * listTxs, size_t numTxs){
    /*1 Verify Header */
    blockHeader tempBlock;
    memcpy(tempBlock.nVersion, blockheader, 4);
    memcpy(tempBlock.hashPreviousBlock, &blockheader[4], 32);
    // extract merkle root
    memcpy(tempBlock.merkleRoot, &blockheader[36], 32);
    memcpy(tempBlock.nTime, &blockheader[68], 4);
    memcpy(tempBlock.nBit,  &blockheader[72], 4);
    memcpy(tempBlock.nNonce, &blockheader[76], 4);
    printf("[+] Verify and Computed Header:\n");
    sgx_sha256_hash_t output; 
    // [todo][very important] verify difficulty here
    doubleSHA256(blockheader, 80, &output); // [todo]
    // check for header start with 000000000
    for (int i = 28; i < 32; i++)
    {
        if(output[i] != 0)
        {
            printf("[-] Not enough proof of work !\n");
            // return -1;
        }
    }
    // [todo] fix this. do not use struct
    uint8_t *transactionHashes = (uint8_t *)malloc(numTxs*32); 
    printf("[+] Verify transaction hashes:\n");
    // copy transaction
    for(int i=0; i<numTxs; i++){
        memcpy(transactionHashes+i*32,listTxs[i].txhash, SGX_SHA256_HASH_SIZE);
    }
    /*verify merkle root*/
    if(verifyMerkleRoot(transactionHashes, numTxs*32, tempBlock.merkleRoot) < 0)
    {
        printf("[-] Merkle root does not match\n");
        // return -1;
    }
    free(transactionHashes);
	return 0;
}

uint32_t getIdFromPkh(unsigned char * pkh, uint32_t max_blocks)
{
	//TODO: error checking
	uint32_t id;
	sgx_sha256_hash_t hash;
    sgx_sha256_msg((uint8_t*)pkh, PKH_SIZE_IN_BYTES, &hash);
    uint32_t num = (hash[0] << 24) | (hash[1] << 16) | (hash[2] << 8) | (hash[3]);
	id = num % max_blocks; //2^n	
	return id;
}

void accessORAM(uint32_t instance_id, uint8_t oram_type, uint32_t block_id, char opType, unsigned char * data_in, unsigned char * data_out){

	//TODO : Would be nice to remove this dynamic allocation.
	PathORAM *poram_current_instance;
	CircuitORAM *coram_current_instance;
	
	if(oram_type==0){
		poram_current_instance = poram_instances[instance_id];
		poram_current_instance->Access_temp(block_id, opType, data_in, data_out);
	}
	else {
		coram_current_instance = coram_instances[instance_id];
		coram_current_instance->Access_temp(block_id, opType, data_in, data_out);
	}
}



int vectorToORAM(uint32_t instance_id, uint8_t oram_type, uint8_t vector_type, uint32_t max_blocks, uint32_t ORAM_block_size, std::vector<txInOut68_t> * vector)
{
	size_t vector_size = (*vector).size();
	printf("vectorToORAM size  \t %d\n", vector_size);
	
	unsigned char data_out[ORAM_block_size], trash[ORAM_block_size];
	uint32_t UTXOsPerORAMblock = ORAM_block_size/68;

	for (size_t item = 0; item<vector_size; item++) {
		printf("Index of vector: %d\n", item);
		// 0. Get block id from pkh
		uint32_t block_id = getIdFromPkh(&(*vector)[item].u68bytes[44], max_blocks);
		
		// 1. Get the Oram block in data_out
		accessORAM(instance_id, oram_type, block_id, 'r', &trash[0], &data_out[0]);
		printf("data_out:\n");	
		hexdump(&data_out[0], ORAM_block_size);

		if(vector_type == 0){ // vin
		// 2. Search for the UTXO that has the input's txhash and position. If found, replace utxo with zeros
			uint32_t index_utxo;
			for(uint32_t utxo=0; utxo<UTXOsPerORAMblock; utxo++){
				index_utxo = utxo*68;
				if(memcmp(&(*vector)[item].u68bytes[0], &data_out[index_utxo], 32)==0 && memcmp(&(*vector)[item].u68bytes[64], &data_out[index_utxo+64], 4)==0){
					printf("UTXO to delete:\n");
					hexdump(&data_out[index_utxo], 68);
					memset(&data_out[index_utxo], 0, 68);
					break;
				}
			}
		}else{ // vout
		// 2. Write the new UTXO into an empty slot of the ORAM block
			uint32_t index_utxo, id;
			for(uint32_t utxo=0; utxo<UTXOsPerORAMblock; utxo++){
				index_utxo = utxo*68;
				id = getIdFromPkh(&data_out[index_utxo+44], max_blocks);
				if(id != block_id){
					printf("UTXO to add:\n");
					hexdump(&(*vector)[item].u68bytes[0], 68);
					memcpy(&data_out[index_utxo], &(*vector)[item].u68bytes[0], 68);
					break;
				}
			}
		}
		// hexdump(&data_out[0], ORAM_block_size);

		// 3. Write the modified block back to the ORAM tree
		accessORAM(instance_id, oram_type, block_id, 'w', &data_out[0], &trash[0]);
				
		// TEST: Read modified block from ORAM tree
		accessORAM(instance_id, oram_type, block_id, 'r', &trash[0], &data_out[0]);
	
		printf("ORAM out:\n");
		for(uint32_t utxo=0; utxo<UTXOsPerORAMblock; utxo++){
			hexdump(&data_out[68*utxo], 68);
		}				
		printf("\n");
	}
}

//Clean up all instances of ORAM on terminate.
int sendRawTxList(uint32_t instance_id, uint8_t oram_type, uint32_t max_blocks, unsigned char * tx, size_t len, size_t numTxs, unsigned char * blockheader, uint32_t blockheight, uint32_t ORAM_block_size)
{
    int offset = 0;
    tx_t listTxs[numTxs];

	std::vector<txInOut68_t> vin;
	std::vector<txInOut68_t> vout;

    for(int i=0; i<numTxs; i++){
        uint32_t txlen = (tx[offset] << 24) | (tx[offset+1] << 16) | (tx[offset+2] << 8) | (tx[offset+3]);
        offset += 4;
        // Calculate double sha256 to build merkle tree
        doubleSHA256(&tx[offset], txlen, &listTxs[i].txhash);
        
        // Assign the transaction hex code
        listTxs[i].hexSize = txlen;
        listTxs[i].hex = (unsigned char *) malloc(txlen);
        memcpy(listTxs[i].hex, &tx[offset], txlen); 
        
        offset += txlen;        
    }
	// Verify
	verifyBlock(blockheader, &listTxs[0], numTxs);

    // Extract UTXOs	
    reducer(listTxs, numTxs, blockheight, &vin, &vout);

	size_t vin_size = vin.size();
	size_t vout_size = vout.size();
	printf("sendRawTxList pruned vin  \t %d\n", vin_size);
	printf("sendRawTxList pruned vout  \t %d\n", vout_size);

	vectorToORAM(instance_id, oram_type, 1, max_blocks, ORAM_block_size, &vout);
	vectorToORAM(instance_id, oram_type, 0, max_blocks, ORAM_block_size, &vout);
	// vectorToORAM(instance_id, oram_type, 0, max_blocks, ORAM_block_size, &vin);
	
	vin.clear();
    vout.clear();

    for(int i=0; i<numTxs; i++){
        // hexdump(listTxs[i].txhash, 32);
        // hexdump(listTxs[i].hex, listTxs[i].hexSize);
        free(listTxs[i].hex);
    }

    
    return 0;
}