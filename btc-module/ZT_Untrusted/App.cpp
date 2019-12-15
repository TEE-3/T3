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

/*
Untrusted Application Code for ZeroTrace
Usage : 
printf("./app <N> <No_of_requests> <Stash_size> <Data_block_size> <"resume"/"new"> <"memory"/"hdd"> <"0"/"1" = Non-oblivious/Oblivious> <Recursion_block_size>");
Note : parameters surrounded by quotes should entered in as is without the quotes.

//META-NOTES :
//_e trailed variables are computed/obtained from enclave
//_p trailed variables are obtained from commandline parameters
*/


#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <pwd.h>
#include <time.h> 
#include "sgx_urts.h"
#include "App.h"
#include "Enclave_u.h"
#include "LocalStorage.hpp"
#include "RandomRequestSource.hpp"

#define MAX_PATH FILENAME_MAX
#define CIRCUIT_ORAM
#define NUMBER_OF_WARMUP_REQUESTS 0
#define ANALYSIS 1
#define MILLION 1E6
#define HASH_LENGTH 32
#define DEBUG_PRINT 1
#define PRINT_REQ_DETAILS 1
#define RESULTS_DEBUG 1

#define RECURSION_LEVELS_DEBUG 1
//#define NO_CACHING_APP 1
//#define EXITLESS_MODE 1
//#define POSMAP_EXPERIMENT 1

// Global Variables Declarations
uint64_t ORAM_INSTANCE_MEM_POSMAP_LIMIT = 1024;
uint32_t MEM_POSMAP_LIMIT = 10 * 1024;
uint64_t PATH_SIZE_LIMIT = 1 * 1024 * 1024;
uint32_t aes_key_size = 16;
uint32_t hash_size = 32;	
#define ADDITIONAL_METADATA_SIZE 24
uint32_t oram_id = 0;

//Timing variables
long mtime, seconds, useconds;
struct timespec time_rec, time_start, time_end, time_pos, time_fetch, upload_start_time , upload_end_time, download_start_time, download_end_time;
struct timespec time3,time4,time5,time2;
double upload_time, download_time;
double t, t1, t2, t3, ut,dt,tf,te;
clock_t ct, ct1, ct2, ct3, cut, cdt;
clock_t ct_pos, ct_fetch, ct_start, ct_end;
LocalStorage ls;
uint32_t recursion_levels_e = 0;

/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;
int ptr;
bool resume_experiment = false;
bool inmem_flag =false;

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

struct oram_request{
	uint32_t *id;
	uint32_t *level;
	uint32_t *d_lev;
	bool *recursion;
	bool *block;
};

struct oram_response{
	unsigned char *path;
	unsigned char *path_hash;
	unsigned char *new_path;
	unsigned char *new_path_hash;
};

struct thread_data{
	struct oram_request *req;
	struct oram_response *resp;
};

struct thread_data td;
struct oram_request req_struct;
struct oram_response resp_struct;	
unsigned char *data_in;
unsigned char *data_out;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    {
        SGX_ERROR_UNEXPECTED,
        "Unexpected error occurred.",
        NULL
    },
    {
        SGX_ERROR_INVALID_PARAMETER,
        "Invalid parameter.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_MEMORY,
        "Out of memory.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_LOST,
        "Power transition occurred.",
        "Please refer to the sample \"PowerTransition\" for details."
    },
    {
        SGX_ERROR_INVALID_ENCLAVE,
        "Invalid enclave image.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ENCLAVE_ID,
        "Invalid enclave identification.",
        NULL
    },
    {
        SGX_ERROR_INVALID_SIGNATURE,
        "Invalid enclave signature.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_EPC,
        "Out of EPC memory.",
        NULL
    },
    {
        SGX_ERROR_NO_DEVICE,
        "Invalid SGX device.",
        "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."
    },
    {
        SGX_ERROR_MEMORY_MAP_CONFLICT,
        "Memory map conflicted.",
        NULL
    },
    {
        SGX_ERROR_INVALID_METADATA,
        "Invalid enclave metadata.",
        NULL
    },
    {
        SGX_ERROR_DEVICE_BUSY,
        "SGX device was busy.",
        NULL
    },
    {
        SGX_ERROR_INVALID_VERSION,
        "Enclave version was invalid.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ATTRIBUTE,
        "Enclave was not authorized.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_FILE_ACCESS,
        "Can't open enclave file.",
        NULL
    },
};

/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret) {
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist/sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++) {
        if(ret == sgx_errlist[idx].err) {
            if(NULL != sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }
    
    if (idx == ttl)
        printf("Error: Unexpected error occurred.\n");
}

/* Initialize the enclave:
 *   Step 1: try to retrieve the launch token saved by last transaction
 *   Step 2: call sgx_create_enclave to initialize an enclave instance
 *   Step 3: save the launch token if it is updated
 */
int initialize_enclave(void) {
    char token_path[MAX_PATH] = {'\0'};
    sgx_launch_token_t token = {0};
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    int updated = 0;
    
    /* Step 1: try to retrieve the launch token saved by last transaction 
     *         if there is no token, then create a new one.
     */
    /* try to get the token saved in $HOME */
    const char *home_dir = getpwuid(getuid())->pw_dir;
    
    if (home_dir != NULL && 
        (strlen(home_dir)+strlen("/")+sizeof(TOKEN_FILENAME)+1) <= MAX_PATH) {
        /* compose the token path */
        strncpy(token_path, home_dir, strlen(home_dir));
        strncat(token_path, "/", strlen("/"));
        strncat(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME)+1);
    } else {
        /* if token path is too long or $HOME is NULL */
        strncpy(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME));
    }

    FILE *fp = fopen(token_path, "rb");
    if (fp == NULL && (fp = fopen(token_path, "wb")) == NULL) {
        printf("Warning: Failed to create/open the launch token file \"%s\".\n", token_path);
    }

    if (fp != NULL) {
        /* read the token from saved file */
        size_t read_num = fread(token, 1, sizeof(sgx_launch_token_t), fp);
        if (read_num != 0 && read_num != sizeof(sgx_launch_token_t)) {
            /* if token is invalid, clear the buffer */
            memset(&token, 0x0, sizeof(sgx_launch_token_t));
            printf("Warning: Invalid launch token read from \"%s\".\n", token_path);
        }
    }
    /* Step 2: call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, &token, &updated, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        if (fp != NULL) fclose(fp);
        return -1;
    }

    /* Step 3: save the launch token if it is updated */
    if (updated == FALSE || fp == NULL) {
        /* if the token is not updated, or file handler is invalid, do not perform saving */
        if (fp != NULL) fclose(fp);
        return 0;
    }

    /* reopen the file with write capablity */
    fp = freopen(token_path, "wb", fp);
    if (fp == NULL) return 0;
    size_t write_num = fwrite(token, 1, sizeof(sgx_launch_token_t), fp);
    if (write_num != sizeof(sgx_launch_token_t))
        printf("Warning: Failed to save launch token to \"%s\".\n", token_path);
    fclose(fp);
    return 0;
}

/* OCall functions */
void ocall_print_string(const char *str) {
    /* Proxy/Bridge will check the length and null-terminate 
     * the input string to prevent buffer overflow. 
     */
    printf("%s", str);
}

void *HandleRequest(void *arg) {
	//printf("In Handle Request thread\n");

	struct thread_data *data;
	data = (struct thread_data *) arg;
	unsigned char *ptr = data->resp->path;
	unsigned char *ptr_hash = data->resp->path_hash;
	uint32_t* id = data->req->id;
	uint32_t *level= data->req->level;
	uint32_t *d_lev = data->req->d_lev;
	bool *recursion = data->req->recursion;
	

	uint64_t path_hash_size = 2 * (*d_lev) * HASH_LENGTH; // 2 from siblings 		

	uint64_t i = 0;

	while(1) {
		//*id==-1 || *level == -1 || 
		while( *(data->req->block) ) {}
		//printf("APP : Recieved Request\n");

		ls.downloadPath(data->resp->path, *id, data->resp->path_hash, path_hash_size, *level , *d_lev);	
		//printf("APP : Downloaded Path\n");	
		*(data->req->block) = true;
				
		while(*(data->req->block)) {}
		ls.uploadPath(data->resp->new_path, *id, data->resp->new_path_hash, *level, *d_lev);
		//printf("APP : Uploaded Path\n");
		*(data->req->block) = true;

		//pthread_exit(NULL);
	}
		
}

uint64_t timediff(struct timeval *start, struct timeval *end) {
	long seconds,useconds;
	seconds  = end->tv_sec  - start->tv_sec;
	useconds = end->tv_usec - start->tv_usec;
	mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
	return mtime;
}

double timetaken(timespec *start, timespec *end){
	long seconds, nseconds;
	seconds = end->tv_sec - start->tv_sec;
	nseconds = end->tv_nsec - start->tv_nsec;
	double mstime = ( double(seconds * 1000) + double(nseconds/MILLION) );
	return mstime;
}

void time_report(uint8_t point) {
	if(point==1) {
		ct_pos = clock();
		clock_gettime(CLOCK_MONOTONIC, &time_pos);
	}
	if(point==2) {
		ct_fetch = clock();
		clock_gettime(CLOCK_MONOTONIC, &time_fetch);
		clock_gettime(CLOCK_MONOTONIC, &time2);
	}
	if(point==3) {
		clock_gettime(CLOCK_MONOTONIC, &time3);
	}
	if(point==4) {
		clock_gettime(CLOCK_MONOTONIC, &time4);
	}
	if(point==5) {
		clock_gettime(CLOCK_MONOTONIC, &time5);
	}
}

uint8_t uploadPath(unsigned char* path_array, uint32_t pathSize, uint32_t leafLabel, unsigned char* path_hash, uint32_t path_hash_size, uint32_t level, uint32_t D_level) {
	clock_t s,e;
	s = clock();
	clock_gettime(CLOCK_MONOTONIC, &upload_start_time);
	ls.uploadPath(path_array,leafLabel,path_hash, level, D_level);
	e = clock();
	clock_gettime(CLOCK_MONOTONIC, &upload_end_time);
	double mtime = timetaken(&upload_start_time, &upload_end_time);

	if(recursion_levels_e >= 1) {
		upload_time += mtime;	
		cut += (e-s);
	}
	else {
		upload_time = mtime;
		cut = (e-s);
	}
	return 1;
}

uint8_t uploadObject(unsigned char* serialized_bucket, uint32_t bucket_size, uint32_t label, unsigned char* hash, uint32_t hashsize, uint32_t size_for_level, uint32_t recursion_level) {
	clock_gettime(CLOCK_MONOTONIC, &upload_start_time);
	ls.uploadObject(serialized_bucket, label, hash, hashsize, size_for_level, recursion_level);
	clock_gettime(CLOCK_MONOTONIC, &upload_end_time);
	double mtime = timetaken(&upload_start_time, &upload_end_time);
	upload_time = mtime;
	//printf("%ld\n",ut);
	return 1;
}

uint8_t downloadPath(unsigned char* path_array, uint32_t pathSize, uint32_t leafLabel, unsigned char *path_hash, uint32_t path_hash_size, uint32_t level, uint32_t D_level) {	
	clock_t s,e;
	s = clock();
	clock_gettime(CLOCK_MONOTONIC, &download_start_time);
	ls.downloadPath(path_array,leafLabel,path_hash, path_hash_size, level, D_level);
	e = clock();	
	clock_gettime(CLOCK_MONOTONIC, &download_end_time);
	double mtime = timetaken(&download_start_time, &download_end_time);
	if(recursion_levels_e >= 1) {
		download_time+= mtime;
		cdt+=(e-s);	
	}
	else {
		download_time = mtime;	
		cdt = (e-s);
	}
	return 1;
}


void build_fetchChildHash(uint32_t left, uint32_t right, unsigned char* lchild, unsigned char* rchild, uint32_t hash_size, uint32_t recursion_level) {
	ls.fetchHash(left,lchild,hash_size, recursion_level);
	ls.fetchHash(right,rchild,hash_size, recursion_level);
}

int8_t computeRecursionLevels(uint32_t max_blocks, uint32_t recursion_data_size, uint64_t onchip_posmap_memory_limit){
    int8_t recursion_levels = -1;
    uint8_t x;
    
    if(recursion_data_size!=0) {		
            recursion_levels = 1;
            x = recursion_data_size / sizeof(uint32_t);
            uint64_t size_pmap0 = max_blocks * sizeof(uint32_t);
            uint64_t cur_pmap0_blocks = max_blocks;

            while(size_pmap0 > onchip_posmap_memory_limit) {
                cur_pmap0_blocks = (uint64_t) ceil((double)cur_pmap0_blocks/(double)x);
                recursion_levels++;
                size_pmap0 = cur_pmap0_blocks * sizeof(uint32_t);
            }

            if(recursion_levels==1)
                recursion_levels=-1;

            #ifdef RECURSION_LEVELS_DEBUG
                printf("IN App: max_blocks = %d\n", max_blocks);
                printf("Recursion Levels : %d\n",recursion_levels);
            #endif
        }
    return recursion_levels;
}

int8_t ZT_Initialize(){
    	// Initialize the enclave 
	if(initialize_enclave() < 0){
		printf("Enter a character before exit ...\n");
		getchar();
		return -1; 
	}
/*
	// Utilize edger8r attributes
	edger8r_array_attributes();
	edger8r_pointer_attributes();
	edger8r_type_attributes();
	edger8r_function_attributes();

	// Utilize trusted libraries 
	ecall_libc_functions();
	ecall_libcxx_functions();
	ecall_thread_functions();
*/

    return 0;
}

void ZT_Close(){
	printf("ZT_Close called\n");
	sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    ret = sgx_destroy_enclave(global_eid);
	if (ret != SGX_SUCCESS) {
        print_error_message(ret);
    }
}
uint32_t ZT_initLocalStorage( uint32_t max_blocks, uint32_t data_size, uint32_t stash_size, uint32_t oblivious_flag, uint32_t recursion_data_size, uint32_t oram_type, uint8_t pZ){
	int8_t recursion_levels;
    
	recursion_levels = computeRecursionLevels(max_blocks, recursion_data_size, MEM_POSMAP_LIMIT);
	// printf("APP.cpp : ComputedRecursionLevels = %d", recursion_levels);
    
	uint32_t D = (uint32_t) ceil(log((double)max_blocks/4)/log((double)2));
	ls.setParams(max_blocks,D,pZ,stash_size,data_size + ADDITIONAL_METADATA_SIZE,inmem_flag, recursion_data_size + ADDITIONAL_METADATA_SIZE, recursion_levels);
}

uint32_t ZT_New( uint32_t max_blocks, uint32_t data_size, uint32_t stash_size, uint32_t oblivious_flag, uint32_t recursion_data_size, uint32_t oram_type, uint8_t pZ){
	sgx_status_t sgx_return = SGX_SUCCESS;
	int8_t rt;
	uint8_t urt;
	uint32_t instance_id;
	int8_t recursion_levels;
	printf("reccccc\n");
    
	recursion_levels = computeRecursionLevels(max_blocks, recursion_data_size, MEM_POSMAP_LIMIT);
	printf("APP.cpp : ComputedRecursionLevels = %d", recursion_levels);
    
	uint32_t D = (uint32_t) ceil(log((double)max_blocks/4)/log((double)2));
	// ls.setParams(max_blocks,D,pZ,stash_size,data_size + ADDITIONAL_METADATA_SIZE,inmem_flag, recursion_data_size + ADDITIONAL_METADATA_SIZE, recursion_levels);
    
	#ifdef EXITLESS_MODE
		int rc;
		pthread_t thread_hreq;
		req_struct.id = (uint32_t*) malloc (4);
		req_struct.level = (uint32_t*) malloc(4);
		req_struct.d_lev = (uint32_t*) malloc(4);
		req_struct.recursion = (bool *) malloc(1);
		req_struct.block = (bool *) malloc(1);

		resp_struct.path = (unsigned char*) malloc(PATH_SIZE_LIMIT);
		resp_struct.path_hash = (unsigned char*) malloc (PATH_SIZE_LIMIT);
		resp_struct.new_path = (unsigned char*) malloc (PATH_SIZE_LIMIT);
		resp_struct.new_path_hash = (unsigned char*) malloc (PATH_SIZE_LIMIT);
		td.req = &req_struct;
		td.resp = &resp_struct;

		*(req_struct.block) = true;
		*(req_struct.id) = 7;

		rc = pthread_create(&thread_hreq, NULL, HandleRequest, (void *)&td);
		if (rc){
		    std::cout << "Error:unable to create thread," << rc << std::endl;
		    exit(-1);
		}
		sgx_return = initialize_oram(global_eid, &urt, max_blocks, data_size,&req_struct, &resp_struct);		
	#else
		//Pass the On-chip Posmap Memory size limit as a parameter.    
		sgx_return = createNewORAMInstance(global_eid, &instance_id, max_blocks, data_size, stash_size, oblivious_flag, recursion_data_size, recursion_levels, MEM_POSMAP_LIMIT, oram_type, pZ);
		//sgx_return = createNewORAMInstance(global_eid, &instance_id, max_blocks, data_size, stash_size, oblivious_flag, recursion_data_size, recursion_levels, MEM_POSMAP_LIMIT, oram_type);
		printf("INSTANCE_ID returned = %d\n", instance_id);
	
		//(uint32_t max_blocks, uint32_t data_size, uint32_t stash_size, uint32_t oblivious_flag, uint32_t recursion_data_size, int8_t recursion_levels, uint64_t onchip_posmap_mem_limit, uint32_t oram_type)
		//sgx_return = createNewORAMInstance(global_eid, &urt, max_blocks, data_size, stash_size, oblivious_flag, recursion_data_size, recursion_levels, MEM_POSMAP_LIMIT, oram_type);
	#endif

    #ifdef DEBUG_PRINT
        printf("initialize_oram Successful\n");
    #endif
    return (instance_id);
}


void ZT_Access(uint32_t instance_id, uint8_t oram_type, uint32_t max_blocks, unsigned char *encrypted_request, unsigned char *encrypted_response, unsigned char *tag_in, unsigned char* tag_out, uint32_t request_size, uint32_t response_size, uint32_t tag_size){
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
	ret = accessInterface(global_eid, instance_id, oram_type, max_blocks, encrypted_request, encrypted_response, tag_in, tag_out, request_size, response_size, tag_size);
    if (ret != SGX_SUCCESS) {
		printf("accessInterface failed, instance_id: %zu", instance_id);
		abort();
	
	}
}

void ZT_Bulk_Read(uint32_t instance_id, uint8_t oram_type, uint32_t no_of_requests, unsigned char *encrypted_request, unsigned char *encrypted_response, unsigned char *tag_in, unsigned char* tag_out, uint32_t request_size, uint32_t response_size, uint32_t tag_size){
    accessBulkReadInterface(global_eid, instance_id, oram_type, no_of_requests, encrypted_request, encrypted_response, tag_in, tag_out, request_size, response_size, tag_size);
}

void Red_SendRawTxList(uint32_t instance_id, uint8_t oram_type, uint32_t max_blocks, unsigned char * tx, size_t len, size_t numTxs, unsigned char * blockheader, uint32_t blockheight, uint32_t ORAM_block_size){
	sgx_status_t status = sendRawTxList(global_eid, &ptr, instance_id, oram_type, max_blocks, tx, len, numTxs, blockheader, blockheight, ORAM_block_size);
	//std::cout << status << std::endl;
	if (status != SGX_SUCCESS) {
		std::cout << "[-] Failed to send rawTx" << std::endl;
	}
}

void test_ORAM(uint32_t instance_id, uint8_t oram_type, uint32_t block_size)
{
  sgx_status_t status = ORAM_testing(global_eid, instance_id, oram_type, block_size);
  if (status != SGX_SUCCESS) {
    std::cout << "FAILED" << std::endl;
  }
}
uint64_t get_recursive_tree_size(uint32_t instance_id, uint32_t oram_type){
	uint64_t size;
	sgx_status_t status = ZT_get_recursive_tree_size(global_eid, &size, instance_id, oram_type);
	if (status != SGX_SUCCESS) {
		std::cout << "get_oram_size() FAILED" << std::endl;
		return -1;
	}
	return size;
}
/*
	uint32_t posmap_size = 4 * max_blocks;
	uint32_t stash_size =  (stashSize+1) * (dataSize_p+8);

*/

/*
if(resume_experiment) {
		
		//Determine if experiment is recursive , and setup parameters accordingly
		if(recursion_data_size!=0) {	
			uint32_t *posmap = (uint32_t*) malloc (MEM_POSMAP_LIMIT*16*4);
			unsigned char *merkle =(unsigned char*) malloc(hash_size + aes_key_size);
			ls.restoreMerkle(merkle,hash_size + aes_key_size);				
			ls.restorePosmap(posmap, MEM_POSMAP_LIMIT*16);
			//Print and test Posmap HERE
			
			//TODO : Fix restoreMerkle and restorePosmap in Enclave :
			//sgx_return = restoreEnclavePosmap(posmap,);			
			for(uint8_t k = 1; k <=recursion_levels_e;k++){
				uint32_t stash_size;
				unsigned char* stash = (unsigned char*) malloc (stash_size);
				//ls.restoreStash();	
				//TODO: Fix restore Stash in Enclave				
				//sgx_return = frestoreEnclaveStashLevel();
				free(stash);						
			}
			
			free(posmap);	
			free(merkle);
			
		}
		else {
		uint32_t current_stashSize = 0;
		uint32_t *posmap = (uint32_t*) malloc (posmap_size);
		uint32_t *stash = (uint32_t*) malloc(4 * 2 * stashSize);
		unsigned char *merkle =(unsigned char*) malloc(hash_size);
		ls.restoreState(posmap, max_blocks, stash, &current_stashSize, merkle, hash_size+aes_key_size);		
		//sgx_return = restore_enclave_state(global_eid, &rt32, max_blocks, dataSize_p, posmap, posmap_size, stash, current_stashSize * 8, merkle, hash_size+aes_key_size);
		//printf("Restore done\n");
		free(posmap);
		free(stash);
		free(merkle);
		}
	}
*/