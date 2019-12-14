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

#include "Main.hpp"
#define MILLION 1E6

uint32_t no_of_elements;
uint32_t no_of_accesses;
uint32_t *element;
uint32_t min_expected_no_of_parameters = 10;
bool resume_experiment;
bool inmem_flag;
uint32_t data_size;
uint32_t max_blocks;
int requestlength;
uint32_t stash_size;
uint32_t oblivious = 0;
uint32_t recursion_data_size = 0;
uint32_t oram_type = 0;
int32_t recursion_levels_e;
uint32_t oram_id = 0;
unsigned char *encrypted_request, *tag_in, *encrypted_response, *tag_out;
uint32_t request_size, response_size;
unsigned char *data_in;
unsigned char *data_out;
uint32_t bulk_batch_size=0;

clock_t generate_request_start, generate_request_stop, extract_response_start, extract_response_stop, process_request_start, process_request_stop, generate_request_time, extract_response_time,  process_request_time;
uint8_t Z;


FILE *iquery_file; 

#define PRINT_REQ_DETAILS 1
#define DEBUG_PRINT 1

std::string username = "haas256";
std::string password = "12345678";
std::string address  = "10.186.98.166"; 
// std::string address  = "25.51.2.246"; 
int port = 8332;
BitcoinAPI btc(username, password, address, port);

double time_taken(timespec *start, timespec *end){
	long seconds, nseconds;
	seconds = end->tv_sec - start->tv_sec;
	nseconds = end->tv_nsec - start->tv_nsec;
	double mstime = ( double(seconds * 1000) + double(nseconds/MILLION) );
	return mstime;
}

int AES_GCM_128_encrypt (unsigned char *plaintext, int plaintext_len, unsigned char *aad,
	int aad_len, unsigned char *key, unsigned char *iv, int iv_len,
	unsigned char *ciphertext, unsigned char *tag)
{
	EVP_CIPHER_CTX *ctx;

	int len;
	int ciphertext_len;

	/* Create and initialise the context */
	if(!(ctx = EVP_CIPHER_CTX_new())) {
		printf("Failed context intialization for OpenSSL EVP\n");
	}

	/* Initialise the encryption operation. */
	if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)){
		printf("Failed AES_GCM_128 intialization for OpenSSL EVP\n");	
	}

	/* Set IV length if default 12 bytes (96 bits) is not appropriate */
	if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL)){
		printf("Failed IV config\n");
	}

	/* Initialise key and IV */
	if(1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv)) {
		printf("Failed intialization for key and IV for AES_GCM\n");	
	}

	/* Provide any AAD data. This can be called zero or more times as
	 * required
	 */
	//if(1 != EVP_EncryptUpdate(ctx, NULL, &len, aad, aad_len))
	//	printf("Failed AAD\n");

	/* Provide the message to be encrypted, and obtain the encrypted output.
	 * EVP_EncryptUpdate can be called multiple times if necessary
	 */
	//printf("Error code = %d\n\n", EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len));
	if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) {
		printf("Failed AES_GCM encrypt\n");
	}
	ciphertext_len = len;

	/* Finalise the encryption. Normally ciphertext bytes may be written at
	 * this stage, but this does not occur in GCM mode
	 */
	if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)){
		printf("Failed Finalizing ciphertext\n");	
	}
	ciphertext_len += len;

	/* Get the tag */
	if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag))
		printf("Failed tag for AES_GCM_encrypt\n");

	/* Clean up */
	EVP_CIPHER_CTX_free(ctx);

	return ciphertext_len;
}

int AES_GCM_128_decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *aad,
	int aad_len, unsigned char *tag, unsigned char *key, unsigned char *iv,
	int iv_len, unsigned char *plaintext)
{
	EVP_CIPHER_CTX *ctx;
	int len;
	int plaintext_len;
	int ret;

	/* Create and initialise the context */
	if(!(ctx = EVP_CIPHER_CTX_new())) 
		printf("Failed context intialization for OpenSSL EVP\n");

	/* Initialise the decryption operation. */
	if(!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL))
		printf("Failed AES_GCM_128 intialization for OpenSSL EVP\n");	

	/* Set IV length. Not necessary if this is 12 bytes (96 bits) */
	if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL))
		printf("Failed IV config\n");

	/* Initialise key and IV */
	if(!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv))
		printf("Failed intialization for key and IV for AES_GCM_128\n");	
	/* Provide any AAD data. This can be called zero or more times as
	 * required
	 */
	//if(!EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len))
	//	printf("Failed AAD\n");

	/* Provide the message to be decrypted, and obtain the plaintext output.
	 * EVP_DecryptUpdate can be called multiple times if necessary
	 */
	if(!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
		printf("Failed AES_GCM decrypt\n");
	plaintext_len = len;

	/* Set expected tag value. Works in OpenSSL 1.0.1d and later */
	if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE, tag))
		printf("Failed tag for AES_GCM_decrypt\n");

	/* Finalise the decryption. A positive return value indicates success,
	 * anything else is a failure - the plaintext is not trustworthy.
	 */
	ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);

	/* Clean up */
	EVP_CIPHER_CTX_free(ctx);

	if(ret > 0)
	{
		/* Success */
		plaintext_len += len;
		return plaintext_len;
	}
	else
	{
		/* Verify failed */
		return -1;
	}
}

void serializeRequest(unsigned char *pkh, char op_type, unsigned char *data, uint32_t data_size, unsigned char* serialized_request){
	unsigned char *request_ptr = serialized_request;
	*request_ptr=op_type;
	request_ptr+=1;
	memcpy(request_ptr, pkh,  PKH_SIZE_IN_BYTES);
	request_ptr+=  PKH_SIZE_IN_BYTES;	
	memcpy(request_ptr, data, data_size);
}


int encryptRequest(unsigned char *pkh, char op_type, unsigned char *data, uint32_t data_size, unsigned char *encrypted_request, unsigned char *tag, uint32_t request_size){
	int encrypted_request_size;	
	//Sample IV;
	unsigned char *iv = (unsigned char *) malloc (IV_LENGTH);
	//1 from op_type
	unsigned char *serialized_request = (unsigned char*) malloc (1+PKH_SIZE_IN_BYTES+data_size);
	serializeRequest(pkh, op_type, data, data_size, serialized_request);

	encrypted_request_size = AES_GCM_128_encrypt(serialized_request, request_size, NULL, 0, (unsigned char*) SHARED_AES_KEY, (unsigned char*) HARDCODED_IV, IV_LENGTH, encrypted_request, tag);
	//printf("encrypted_request_size returned for AES_GCM_128_encrypt = %d\n", encrypted_request_size);

	free(serialized_request);
	free(iv);
	return encrypted_request_size;
}

int encryptBulkReadRequest(int *rs, uint32_t req_counter, uint32_t bulk_batch_size, unsigned char *encrypted_request, unsigned char *tag, uint32_t request_size ){
	int encrypted_request_size;	
	//Sample IV;
	unsigned char *iv = (unsigned char *) malloc (IV_LENGTH);
	unsigned char *serialized_request = (unsigned char*) malloc (encrypted_request_size);
	unsigned char *serialized_request_ptr = serialized_request;
	for(int i =0;i<bulk_batch_size;i++){
		memcpy(serialized_request_ptr, &(rs[req_counter+i]), ID_SIZE_IN_BYTES);
		serialized_request_ptr+=ID_SIZE_IN_BYTES;
	}

	encrypted_request_size = AES_GCM_128_encrypt(serialized_request, request_size, NULL, 0, (unsigned char*) SHARED_AES_KEY, (unsigned char*) HARDCODED_IV, IV_LENGTH, encrypted_request, tag);
	//printf("encrypted_request_size returned for AES_GCM_128_encrypt = %d\n", encrypted_request_size);

	free(serialized_request);
	free(iv);
	return encrypted_request_size;

}

void decryptBulkReadRequest(uint32_t bulk_batch_size, unsigned char *encrypted_request, unsigned char *tag, uint32_t request_size){
	unsigned char *decrypted_request = (unsigned char*) malloc (request_size);
	int decrypted_request_size = AES_GCM_128_decrypt(encrypted_request, request_size, NULL, 0, tag, (unsigned char *) SHARED_AES_KEY,(unsigned char*) HARDCODED_IV, IV_LENGTH, decrypted_request);	
	uint32_t request_id;
	uint32_t *req_id = (uint32_t*) decrypted_request;
	for(int i =0;i<bulk_batch_size;i++){	
		printf("Decrypted request id = %d\n", *req_id);
		req_id++;
	}		
	free(decrypted_request);
}


uint32_t computeCiphertextSize(uint32_t data_size){
	//Rounded up to nearest block size:
	uint32_t encrypted_request_size;
	//encrypted_request_size = ((1+ID_SIZE_IN_BYTES+data_size) / AES_GCM_BLOCK_SIZE_IN_BYTES);
	//if((ID_SIZE_IN_BYTES+data_size)%AES_GCM_BLOCK_SIZE_IN_BYTES!=0)
	encrypted_request_size = ((1+PKH_SIZE_IN_BYTES+data_size) / AES_GCM_BLOCK_SIZE_IN_BYTES);
	if((PKH_SIZE_IN_BYTES+data_size)%AES_GCM_BLOCK_SIZE_IN_BYTES!=0)
		encrypted_request_size+=1;
	encrypted_request_size*=16;
	printf("Request_size = %d\n", encrypted_request_size);
	return encrypted_request_size;
}

uint32_t computeBulkRequestsCiphertextSize(uint32_t bulk_batch_size) {
	uint32_t encrypted_request_size;
	encrypted_request_size = ((ID_SIZE_IN_BYTES*bulk_batch_size) / AES_GCM_BLOCK_SIZE_IN_BYTES);
	if((ID_SIZE_IN_BYTES*bulk_batch_size)%AES_GCM_BLOCK_SIZE_IN_BYTES!=0)
		encrypted_request_size+=1;
	encrypted_request_size*=AES_GCM_BLOCK_SIZE_IN_BYTES;
	printf("Request_size = %d\n", encrypted_request_size);
	return encrypted_request_size;
}

void decryptRequest(unsigned char *encrypted_request, unsigned char *tag, uint32_t request_size){
	printf("Encrypted payload = %s", encrypted_request);
	unsigned char *decrypted_request = (unsigned char*) malloc (request_size);
	int decrypted_request_size = AES_GCM_128_decrypt(encrypted_request, request_size, NULL, 0, tag, (unsigned char *) SHARED_AES_KEY,(unsigned char*) HARDCODED_IV, IV_LENGTH, decrypted_request);	
	uint32_t request_id;
	memcpy(&request_id, decrypted_request+1, 4);
	printf("Decrypted request id = %d\n", request_id);
	printf("Decrypted payload = %s", (decrypted_request + 4));
	free(decrypted_request);

}

int extractResponse(unsigned char *encrypted_response, unsigned char *tag, int response_size, unsigned char *data_out) {
	AES_GCM_128_decrypt(encrypted_response, response_size, NULL, 0, tag, (unsigned char*) SHARED_AES_KEY, (unsigned char*) HARDCODED_IV, IV_LENGTH, data_out);
	return response_size;
}

int extractBulkResponse(unsigned char *encrypted_response, unsigned char *tag, int response_size, unsigned char *data_out) {
	AES_GCM_128_decrypt(encrypted_response, response_size, NULL, 0, tag, (unsigned char*) SHARED_AES_KEY, (unsigned char*) HARDCODED_IV, IV_LENGTH, data_out);
	return response_size;
}

void getParams(int argc, char* argv[])
{
	if(argc<min_expected_no_of_parameters) {
		printf("Command line parameters error, expected :\n");
		printf(" <N> <No_of_requests> <Stash_size> <Data_block_size> <\"resume\"/\"new\"> <\"memory\"/\"hdd\"> <0/1 = Non-oblivious/Oblivious> <Recursion_block_size> <\"auto\"/\"path\"/\"circuit\"> <Z>\n\n");
	}

	std::string str = argv[1];
	max_blocks = std::stoi(str);
	str = argv[2];
	requestlength = std::stoi(str);
	str = argv[3];
	stash_size = std::stoi(str);
	str = argv[4];
	data_size = std::stoi(str);
	str = argv[5];
	if(str=="resume")
		resume_experiment = true;
	str = argv[6];
	if(str=="memory")
		inmem_flag = true;
	str = argv[7];
	if(str=="1")
		oblivious = 1;
	str = argv[8];	
	recursion_data_size = std::stoi(str);
	str = argv[9];
	if(str=="path")
		oram_type = 0;
	if(str=="circuit")
		oram_type = 1;
	str=argv[10];
		Z = std::stoi(str);
	str=argv[11];
		bulk_batch_size = std::stoi(str);

	std::string qfile_name = "ZT_"+std::to_string(max_blocks)+"_"+std::to_string(data_size);
	iquery_file = fopen(qfile_name.c_str(),"w");
}


struct node{
	uint32_t id;
	uint32_t data;
	struct node *left, *right;
};


/* OCall functions */
void ocall_print_string(const char *str) {
    /* Proxy/Bridge will check the length and null-terminate 
     * the input string to prevent buffer overflow. 
     */
    printf("%s", str);
}

void hexdump(unsigned char *a, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++)
        printf("%02x", a[i]);
    printf("\n");
}

bool StringToHex(const std::string &inStr, uint8_t *outStr)
{
    size_t len = inStr.length();
    for (size_t i = 0; i < len; i += 2) {
        sscanf(inStr.c_str() + i, "%2hhx", outStr);
        ++outStr;        
    }
    return true;
}

std::string reverse_pairs(std::string const & src)
{
    assert(src.size() % 2 == 0);
    std::string result;
    result.reserve(src.size());

    for (std::size_t i = src.size(); i != 0; i -= 2)
    {
        result.append(src, i - 2, 2);
    }
    return result;
}
/*
BitcoinAPI callBitcoinDaemon()
{
	std::string username = "haas256";
    std::string password = "12345678";
    std::string address  = "10.186.98.166";
    int port = 8332;
    BitcoinAPI btc(username, password, address, port);
	return btc;    
}
*/

blockinfo_t getLatestBitcoinBlock()
{
	
	// get current number of Bitcoin blocks
	int blockcount = btc.getblockcount();

	// get the latest block
	std::string blockHash = btc.getblockhash(blockcount);

	// get block 	
	blockinfo_t currentBlock = btc.getblock(blockHash, 2);        

	return currentBlock;
}

blockinfo_t getBitcoinBlock(int block)
{	
	// get the block hash
	std::string blockHash = btc.getblockhash(block);  

	// get block 
	blockinfo_t currentBlock = btc.getblock(blockHash, 2);        

	return currentBlock;	
}

void getTxList(blockinfo_t currentBlock, size_t numOfTxs, unsigned char * txsListChar)
{
	uint32_t offset=0;
        
	for(size_t i=0; i< numOfTxs; i++)
	{
		std::stringstream txlen;
		txlen << std::setfill ('0') << std::setw(8) << std::hex << currentBlock.tx[i].length()/2;
		StringToHex(txlen.str(), &txsListChar[offset]);
		offset+=4;
		StringToHex(currentBlock.tx[i], &txsListChar[offset]);
		offset+=currentBlock.tx[i].length()/2;
	}   
}

bool getBitcoinBlockHeader(blockinfo_t currentBlock, uint8_t * blockheader)
{
	printf("[+] Parsing Header\n");
	/*******************************************************************/
	//TODO clean this code
	// get block header
	// nVersion||HashPrevBlock||HashMerkleRoot||nTime||nBits||nNonce
	std::stringstream blkheader;
	std::stringstream bitcoinHeader;
	// blkheader = nVersion 
	blkheader     << std::setfill ('0') << std::setw(8) << std::hex << currentBlock.version;
	// bitcoinHeader = reverse(blkheader) = reverse(nVersion) 
	bitcoinHeader << reverse_pairs(blkheader.str());
	// blkheader = ""
	blkheader.str(std::string());
	// bitcoinHeader = reverse(blkheader) = reverse(nVersion)||reverse(previousblockhash)||reverse(merkleroot)
	//                                            4                       32                       32
	bitcoinHeader << reverse_pairs(currentBlock.previousblockhash);
	bitcoinHeader << reverse_pairs(currentBlock.merkleroot) ;
	// blkheader = time 
	blkheader     << std::setfill ('0') << std::setw(8) << std::hex << currentBlock.time;
	// bitcoinHeader = reverse(blkheader) = reverse(nVersion)||reverse(previousblockhash)||reverse(merkleroot)|| reverse(time) 
	//                                            4                       32                       32                4
	bitcoinHeader << reverse_pairs(blkheader.str());
	// blkheader = ""
	blkheader.str(std::string());
	// bitcoinHeader = reverse(blkheader) = reverse(nVersion)||reverse(previousblockhash)||reverse(merkleroot)|| reverse(time) || reverse(bit)
	//                                            4                       32                       32                4               4
	bitcoinHeader << reverse_pairs(currentBlock.bits);

	// blkheader = nonce    
	blkheader     << std::setfill ('0') << std::setw(8) << std::hex << currentBlock.nonce;
	// bitcoinHeader = reverse(blkheader) = reverse(nVersion)||reverse(previousblockhash)||reverse(merkleroot)|| reverse(time) || reverse(bit) || reverse(nonce)
	//                                            4                       32                       32                4               4                  4
	bitcoinHeader << reverse_pairs(blkheader.str());
	// blkheader = ""
	blkheader.str(std::string());
	/***********************************************************************************************************/

	StringToHex(bitcoinHeader.str(), blockheader);

	return true;
}


uint32_t makeBlockID(unsigned char *pkh){
	unsigned char *hash = (unsigned char*) malloc(32);
	SHA256(pkh,20,hash);
	uint32_t num = (hash[0] << 24) | (hash[1] << 16) | (hash[2] << 8) | (hash[3]);	
	return num % 2097152; 
}

/*
int main(int argc, char *argv[]) 
{
	getParams(argc, argv);

	ZT_Initialize();
	uint32_t zt_id = ZT_New(max_blocks, data_size, stash_size, oblivious, recursion_data_size, oram_type, Z);

	// Variable declarations
	clock_t start,end,tclock;	
	uint32_t WriteUtxoNum = 2; //LZ: number of write accesses
	uint32_t ReadUtxoNum = 2;  //LZ: number of read accesses (for testing)
	
	// request_size = ID_SIZE_IN_BYTES + data_size;
  uint32_t request_size = PKH_SIZE_IN_BYTES + data_size;
	uint32_t encrypted_request_size = computeCiphertextSize(data_size);

	response_size = data_size;
	data_out = (unsigned char*) malloc (data_size);
	encrypted_request = (unsigned char *) malloc (encrypted_request_size);				
	encrypted_response = (unsigned char *) malloc (response_size);
	
  // misc
	tag_in = (unsigned char*) malloc (TAG_SIZE);
	tag_out = (unsigned char*) malloc (TAG_SIZE);
	data_in = (unsigned char*) malloc (data_size);

		
	// *	-----------------------------------------------------------------
	// * 	LZ: Loading UTXO file
	// *	The file is designed to contain blocks of data_size bytes and all 
	// *	transactions in a single block have the same pkh.
	// *	-----------------------------------------------------------------
		

	FILE *utxo_fd;
	int r;
	utxo_fd = fopen("biggerUTXO", "r+");
	if(utxo_fd == NULL){
		perror("Error opening the utxo file");
		exit(-1);
	}
							
	start = clock();

  // debugging
  printf("encrypted_request_size: %d, data_size: %d\n", encrypted_request_size, data_size);	

	#ifdef DEBUG_PRINT	
		printf("(Step #1). Writing UTXOs into the ORAM Tree\n");
	#endif

	//TODO: Patch this along with instances patch		
	uint32_t instance_id = 0;	
  	int curUtxo = 0;

	//LZ: this loop reads from the file utxo_fd until there's no more data to read (end of file)
	while(fread(data_in, data_size, 1, utxo_fd)!=0 && curUtxo<WriteUtxoNum){
    curUtxo++;

		#ifdef PRINT_REQ_DETAILS		
      printf ("Inserting UTXO #%d\n", curUtxo);
		#endif
		
		//Prepare Request:
		generate_request_start = clock();
		
		//Get pkh and use it as the id of the request
		unsigned char pkh[PKH_SIZE_IN_BYTES];
		memcpy(&pkh[0], &data_in[44], PKH_SIZE_IN_BYTES);
		uint32_t id = makeBlockID(pkh);
		printf("pkh_id: %u\n", id);

		// LZ: we slightly changed the way ZeroTrace encrypt the read/write request to a specific block. ZeroTrace  
		// used the id of the block as a parameter of the encryptRequest function in order to read/write to the block,
		// we now use the public key hash (pkh).
		// Since the client is not supposed to know the block id, instead of using the it directly, we send the pkh
		// of the first transaction of each block of data_size bytes (assuming all the txs of that block map to 
		// the same BTC_block) to encryptRequest(), then we send the resulting encrypted_request to ZT_Access(). Inside
		// ZT_Access() we apply the sha256 to pkh, take the first 32 bytes of the digest and cast them to an unsigned
		// integer. That integer is design to be 0 for the first block of the biggerUTXO file, 1 for the second block
		// and so on, then we used it as the ORAM block id.

		encryptRequest(pkh, 'w', data_in, data_size, encrypted_request, tag_in, encrypted_request_size);
		generate_request_stop = clock();

		// Process Request:
		process_request_start = clock();		
		ZT_Access(instance_id, oram_type, encrypted_request, encrypted_response, tag_in, tag_out,
              encrypted_request_size, response_size, TAG_SIZE);
		process_request_stop = clock();				

		// Extract Response:
		extract_response_start = clock();
		extractResponse(encrypted_response, tag_out, response_size, data_out);
		extract_response_stop = clock();

		#ifdef ANALYSIS
			//TIME in CLOCKS
			generate_request_time = generate_request_stop - generate_request_start;
			process_request_time = process_request_stop - process_request_start;			
			extract_response_time = extract_response_stop - extract_response_start;
			fprintf(iquery_file,"%f\t%f\t%f\n", double(generate_request_time)/double(CLOCKS_PER_MS),
        double(process_request_time)/double(CLOCKS_PER_MS), double(extract_response_time)/double(CLOCKS_PER_MS));
		
			#ifdef NO_CACHING_APP
				system("sudo sync");
				system("sudo echo 3 > /proc/sys/vm/drop_caches");
			#endif
		#endif

	}

	fclose(utxo_fd);
	printf("Finished inserting %d UTXOs\n", WriteUtxoNum);
  printf("----------------------------\n");

	// LZ. For testing purposes, we extracted the first pkh of biggerUTXO file and perform a read operation to the ORAM
	for(ReadUtxoNum=0; ReadUtxoNum<WriteUtxoNum; ReadUtxoNum++){
		  //Prepare Request:
		  unsigned char jpkh[20] = {0x7d, 0xc8, 0x59, 0x4c, 0xa5, 0xdb, 0x7e,
                    0x7d, 0x3e, 0x01, 0x7d, 0x78, 0x59, 0xdd, 0xd5, 0xa6, 0x42, 0x31, 0xb7, 0x99};

		  encryptRequest(jpkh, 'r', data_in, data_size, encrypted_request, tag_in, encrypted_request_size);

		  //Process Request:		
		  ZT_Access(instance_id, oram_type, encrypted_request, encrypted_response, tag_in, tag_out,
                encrypted_request_size, response_size, TAG_SIZE);
		
		  //Extract Response:
		  extractResponse(encrypted_response, tag_out, response_size, data_out);

      // Debugging
      printf("Reading UTXO #%d\n", ReadUtxoNum);
		
		  // Show the hex codes
		  // hexdump(data_out, response_size);		
	}
	
	#ifdef ANALYSIS
		fclose(iquery_file);
	#endif

	end = clock();
	tclock = end - start;

	//Time in CLOCKS :
	// if(bulk_batch_size==0)
	// 	printf("Per query time = %f ms\n",(1000 * ( (double)tclock/ ( (double)i+j) ) / (double) CLOCKS_PER_SEC));	
	// else
	// 	printf("Per query time = %f ms\n",(1000 * ( (double)tclock/ ( (double)i+j) ) / (double) CLOCKS_PER_SEC));
	
	free(encrypted_request);
	free(encrypted_response);
	free(tag_in);
	free(tag_out);
	free(data_in);
	free(data_out);

	return 0;
}
*/

int main(int argc, char *argv[]) {

	getParams(argc, argv);

	ZT_Initialize();
	uint32_t zt_id = ZT_New(max_blocks, data_size, stash_size, oblivious, recursion_data_size, oram_type, Z);

#ifdef ORAM_TESTING
  // ADIL.
  printf("Testing the ORAM implementation\n");
  test_ORAM(0, oram_type, data_size);
#else

	// Variable declarations
	clock_t start,end,tclock;	
	uint32_t WriteUtxoNum = 2; //LZ: number of write accesses
	uint32_t ReadUtxoNum = 2;  //LZ: number of read accesses (for testing)
	
	// request_size = ID_SIZE_IN_BYTES + data_size;
  uint32_t request_size = PKH_SIZE_IN_BYTES + data_size;
	uint32_t encrypted_request_size = computeCiphertextSize(data_size);

	response_size = data_size;
	data_out = (unsigned char*) malloc (data_size);

	start = clock();

	#ifdef DEBUG_PRINT	
		printf("(Step #1). Writing UTXOs into the ORAM Tree\n");
	#endif

	//TODO: Patch this along with instances patch		
	uint32_t instance_id = 0;	
  	int curUtxo = 0;

	for(int block = 0; block <= 0; block++)
    {		
		blockinfo_t currentBlock = getBitcoinBlock(requestlength);
		// blockinfo_t currentBlock = getLatestBitcoinBlock();

		unsigned char b[4];
		memcpy(&b[0], &currentBlock.height, 4);        

		printf("Block #%d\n", currentBlock.height);

		hexdump(&b[0], 4);

		// get number of transactions
		size_t numOfTxs = currentBlock.tx.size();

		size_t  listLen = currentBlock.size + 4*numOfTxs;
		unsigned char * txsListChar = (uint8_t*) malloc(listLen);
		getTxList(currentBlock, numOfTxs, txsListChar);

		uint8_t blockheader[80];
		getBitcoinBlockHeader(currentBlock, &blockheader[0]);


		Red_SendRawTxList(instance_id, oram_type, max_blocks, txsListChar, listLen, numOfTxs, blockheader, (uint32_t) currentBlock.height, data_size);
        
        free(txsListChar);

        printf("\n----------------------------------------------------------------\n");
	}

#endif

	return 0;	

}

