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
LocalStorage.cpp
*/

#include "Enclave_u.h"
#include "LocalStorage.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>



//uint64_t MEM_POSMAP_LIMIT_LS = 1 * 1024;
uint64_t MEM_POSMAP_LIMIT_LS =  1 * 1024;
//uint32_t MEM_POSMAP_LIMIT_LS = 32 * (4);
uint64_t RAM_LIMIT = (uint64_t)(60 * 1024) * (uint64_t)(1024 * 1024);





/*
Debug Module (Auxiliary Snippet) : For Block level debugging on Storage side

uint32_t extDataSize = 1024 - 18;
uint32_t gN = 128;
uint32_t noncelen = 10;

class Block {
	public:
		unsigned char *data;
		uint32_t id;
		uint32_t treeLabel;
		uint8_t *r;

	Block(){
		data = NULL;
		r = NULL;
		treeLabel = 0;
		id = gN;
	}

	Block(Block &b)
	{
		data = (unsigned char*) malloc (extDataSize);
		r = (uint8_t*) malloc(noncelen);
		memcpy(r,b.r,noncelen);
		memcpy(data,b.data,extDataSize);

		treeLabel = b.treeLabel;
		id = b.id;
	}

	Block(uint32_t set_id, uint8_t *set_data, uint32_t set_label){
		//data = ; Generate datablock of appropriate datasize for new block.
		//treeLabel = 0;
		id = set_id;
		data = set_data;
		treeLabel = set_label;
	}

	Block(unsigned char* serialized_block){
		unsigned char* ptr = serialized_block;
		r = (uint8_t*) malloc(noncelen);

		memcpy(r,ptr,noncelen);
		ptr+= noncelen;

		memcpy((void *)&id, ptr, 4);
		ptr+=4;
		memcpy((void *)&treeLabel, ptr, 4);
		ptr+=4;

		data = (unsigned char*) malloc (extDataSize);
		memcpy(data,ptr,extDataSize);
		ptr+= extDataSize;
	}

	void fill (unsigned char* serialized_block)
	{
		unsigned char* ptr = serialized_block;
		r = (uint8_t*) malloc(noncelen);

		memcpy(r,ptr,noncelen);
		ptr+= noncelen;

		memcpy((void *)&id, ptr, 4);
		ptr+=4;
		memcpy((void *)&treeLabel, ptr, 4);
		ptr+=4;

		data = (unsigned char*) malloc (extDataSize);
		memcpy(data,ptr,extDataSize);
		ptr+= extDataSize;
	}

	~Block()
	{
		if(r)
			free(r);
		if(data)
			free(data);
	}

	unsigned char* serialize() {
		unsigned char* serialized_block = (unsigned char*) malloc(dataSize);
		unsigned char *ptr = serialized_block;
		memcpy(ptr,(void *) r,noncelen);
		ptr+=noncelen;
		memcpy(ptr,(void *) &id,sizeof(id));
		ptr+=sizeof(id);
		memcpy(ptr,(void *) &treeLabel,sizeof(treeLabel));
		ptr+=sizeof(treeLabel);
		memcpy(ptr,data,extDataSize);
		ptr+=extDataSize;
		return serialized_block;
	}

	void serializeToBuffer(unsigned char* serialized_block) {
		unsigned char *ptr = serialized_block;
		memcpy(ptr,(void *) r,noncelen);
		ptr+=noncelen;
		memcpy(ptr,(void *) &id,sizeof(id));
		ptr+=sizeof(id);
		memcpy(ptr,(void *) &treeLabel,sizeof(treeLabel));
		ptr+=sizeof(treeLabel);
		memcpy(ptr,data,extDataSize);
		ptr+=extDataSize;
	}

};
*/

LocalStorage::LocalStorage(){
}

void LocalStorage::setParams(uint32_t maxBlocks,uint32_t set_D, uint32_t set_Z, uint32_t stashSize, uint32_t dataSize_p, bool inmem_p, uint32_t recursion_block_size, int8_t recursion_levels_p)
{
	//Test and set directory name
	dataSize = dataSize_p;
	D = set_D;
	Z = set_Z;
	inmem = inmem_p;	
	recursionBlockSize = recursion_block_size;
	recursion_levels = recursion_levels_p;

	bucket_size = dataSize_p * Z;
	datatree_size = (pow(2,D+1)-1) * (bucket_size);
	hashtree_size = ((pow(2,D+1)-1) * (HASH_LENGTH));

	#ifdef DEBUG_LS
		printf("\nIN LS : recursion_levels = %d, dataSize = %d, recursionBlockSize = %d\n\n",recursion_levels, dataSize, recursionBlockSize);
		printf("DataTree_size = %ld or %f GB, HashTree_size = %ld or %f GB\n",datatree_size,float(datatree_size)/(float(1024*1024*1024)),hashtree_size,float(hashtree_size)/(float(1024*1024*1024)));
	#endif	

    if(recursion_levels==-1) {
        #ifdef DEBUG_LS			
            printf("DataTree_size = %ld, HashTree_size = %ld\n",datatree_size,hashtree_size);			
        #endif			
        inmem_tree = (unsigned char *) malloc(datatree_size);
        inmem_hash = (unsigned char *) malloc(hashtree_size);
    }	
    else {	
        #ifdef RESUME_EXPERIMENT
            //Resuming mechanism for in-memory database
        #else	
        
            uint32_t x = (recursion_block_size - 24) / 4;
            #ifdef DEBUG_LS
                printf("X = %d\n",x);
            #endif
            uint64_t pmap0_blocks = maxBlocks; 				
            uint64_t cur_pmap0_blocks = maxBlocks;
            uint64_t * maxBlocks_of_pmap_level = (uint64_t*) malloc((recursion_levels +1) * sizeof(uint64_t*));
        
            uint32_t level = recursion_levels;
            maxBlocks_of_pmap_level[recursion_levels] = pmap0_blocks;
        
    
            while(level > 1) {
                maxBlocks_of_pmap_level[level-1] = ceil((double)maxBlocks_of_pmap_level[level]/(double)x);
                level--;
            }
            maxBlocks_of_pmap_level[0] = maxBlocks_of_pmap_level[1];
            #ifdef DEBUG_LS
                printf("LS:Level : %d, Blocks : %ld\n", 0, maxBlocks_of_pmap_level[0]);	
            #endif	
            level = 2;
        
            while(level <= recursion_levels) {
                maxBlocks_of_pmap_level[level] = maxBlocks_of_pmap_level[level-1] * x;
                #ifdef DEBUG_LS
                    printf("LS:Level : %d, Blocks : %ld\n", level, maxBlocks_of_pmap_level[level]);
                #endif				
                level++;
            }
            printf("Malloc?\n");
            inmem_tree_l = (unsigned char**) malloc ((recursion_levels+1)*sizeof(unsigned char*));
            inmem_hash_l = (unsigned char**) malloc ((recursion_levels+1)*sizeof(unsigned char*));
	    uint64_t sum_size_inmem_tree_l = 0;
            uint64_t sum_size_inmem_hash_l = 0;
            for(uint32_t i = 1;i<= recursion_levels;i++) {
                uint64_t level_size; 
                if(i==recursion_levels)	
                    level_size = 2 * ceil((double)maxBlocks_of_pmap_level[i])*(Z*(dataSize_p+ADDITIONAL_METADATA_SIZE)); 
                else
                    level_size = 2 * ceil((double) maxBlocks_of_pmap_level[i]) * (Z*(recursion_block_size+ADDITIONAL_METADATA_SIZE));
                // uint32_t pD_temp = ceil((double)maxBlocks_of_pmap_level[i]/(double) UTILIZATION_PARAMETER);
                // uint32_t pD = (uint32_t) ceil(log((double)pD_temp)/log((double)2));
                uint64_t hashtree_size_this = 2 * maxBlocks_of_pmap_level[i] * HASH_LENGTH;				
            
                //Setup Memory locations for hashtree and recursion block	
                inmem_tree_l[i] = (unsigned char*) malloc(level_size);
                if (inmem_tree_l[i]==NULL) exit (1);
		        sum_size_inmem_tree_l += level_size;
                inmem_hash_l[i] = (unsigned char*) malloc(hashtree_size_this);
                if (inmem_hash_l[i]==NULL) exit (1);
		        sum_size_inmem_hash_l += hashtree_size_this;
            }
	        #ifdef DEBUG_LS
                printf("Malloc DataTree_size = %lu or %f GB, HashTree_size = %lu or %f GB\n",sum_size_inmem_tree_l,double(sum_size_inmem_tree_l)/(double(1024*1024*1024)),sum_size_inmem_hash_l,double(sum_size_inmem_hash_l)/(double(1024*1024*1024)));
            #endif
            free(maxBlocks_of_pmap_level);			
        #endif
    }
}

LocalStorage::LocalStorage(LocalStorage&ls)
{
}

void LocalStorage::connect()
{
	
}

void LocalStorage::fetchHash(uint32_t objectKey, unsigned char* hash, uint32_t hashsize, uint32_t recursion_level) {
	
    if(recursion_level!=-1) {
        memcpy(hash,inmem_hash_l[recursion_level]+((objectKey-1)*HASH_LENGTH), HASH_LENGTH);		
    }
    else {
        memcpy(hash,inmem_hash+((objectKey-1)*HASH_LENGTH), HASH_LENGTH);
    }
	
}

uint8_t LocalStorage::uploadObject(unsigned char *data, uint32_t objectKey,unsigned char *hash, uint32_t hashsize, uint32_t size_for_level, uint32_t recursion_level)
{
	uint64_t pos;

    if(recursion_level == -1) {
        memcpy(inmem_tree+((Z*size_for_level)*(objectKey-1)),data,(size_for_level*Z));
        memcpy(inmem_hash+(HASH_LENGTH*(objectKey-1)),hash,HASH_LENGTH);
    }
    else {
        uint64_t pos = ((uint64_t)(Z*size_for_level))*((uint64_t)(objectKey-1));
        memcpy(inmem_tree_l[recursion_level]+(pos),data,(size_for_level*Z));
        memcpy(inmem_hash_l[recursion_level]+(HASH_LENGTH*(objectKey-1)),hash,HASH_LENGTH);
    }	
	
	return 0;
}

uint8_t LocalStorage::uploadPath(unsigned char *path, uint32_t leafLabel,unsigned char *path_hash, uint32_t level, uint32_t D_level)
{
	uint32_t size_for_level = dataSize;
	uint64_t pos;
	
    if(level==recursion_levels)
        size_for_level = dataSize;
    else
        size_for_level = recursionBlockSize;
	

	uint32_t temp = leafLabel;
	unsigned char* path_iter = path;
	unsigned char* path_hash_iter = path_hash;


    if(level == -1) {
        for(uint8_t i = 0;i<D_level+1;i++) {
            memcpy(inmem_tree+(bucket_size*(temp-1)),path_iter,bucket_size);
            memcpy(inmem_hash+(HASH_LENGTH*(temp-1)),path_hash_iter,HASH_LENGTH);
            path_iter += bucket_size;
            path_hash_iter+=HASH_LENGTH;	
            temp = temp>>1;		
        }	
    }
    else {
        //printf("size_for_level = %d\n",size_for_level);	
        for(uint8_t i = 0;i<D_level+1;i++) {
            memcpy(inmem_tree_l[level]+((Z*size_for_level)*(temp-1)),path_iter,(Z*size_for_level));
            #ifndef PASSIVE_ADVERSARY				
                memcpy(inmem_hash_l[level]+(HASH_LENGTH*(temp-1)),path_hash_iter,HASH_LENGTH);
                /*
                printf("LS_UploadPath : Level = %d, Bucket no = %d, Hash = ",level, temp);
                for(uint8_t l = 0;l<HASH_LENGTH;l++)
                    printf("%c",(*(path_hash_iter + l) %26)+'A');
                printf("\n");
                */
                path_hash_iter+=HASH_LENGTH;
            #endif
            
            /*
            //DEBUG module				
            uint32_t* iter_ptr = (uint32_t*) path_iter;
            for(uint8_t q = 0; q<Z; q++){
                printf("in LS (uploadPath) : (%d,%d)\n",iter_ptr[4],iter_ptr[5]);
            
                iter_ptr += 6;
                for(uint8_t j = 0 ; j<16;j++){
                    printf("%d,", *iter_ptr);
                    iter_ptr+=1;
                }
                printf("\n");
                
                iter_ptr+=22;
            }
            */	
            
            
            path_iter+=(Z*size_for_level);	
            temp = temp>>1;		

        }
        //printf("UploadPath success\n");
        
    }	
	
	
	/*
	#ifdef FILESTREAM_MODE
		#ifdef NO_CACHING
			if(level == recursion_levels && inmem==false) {
				system("sudo sync");
			}
		#endif
	#endif
	*/
	return 0;
}

/*
LocalStorage::downloadPath() - returns requested path in *path

Requested path is returned leaf to root.
For each node on path, returned path_hash contains <L-hash, R-Hash> pairs with the exception of a single hash for root node

*/

unsigned char* LocalStorage::downloadPath(unsigned char* path, uint32_t leafLabel, unsigned char *path_hash, uint32_t path_hash_size,uint32_t level, uint32_t D_lev)
{
	uint64_t pos;
	uint32_t size_for_level = dataSize;

    if(level==-1) {
        size_for_level = dataSize;
    }
    else{
        if(level==recursion_levels)
            size_for_level = dataSize;
        else
            size_for_level = recursionBlockSize;
    }


	uint32_t temp = leafLabel;
	unsigned char* path_iter = path;
	unsigned char* path_hash_iter = path_hash;	


    if(level == -1) {
        for(uint8_t i = 0;i<D+1;i++) {
            memcpy(path_iter,inmem_tree+((Z*size_for_level)*(temp-1)),(Z*size_for_level));
            #ifndef PASSIVE_ADVERSARY
                memcpy(path_hash_iter, inmem_hash+(HASH_LENGTH*(temp-1)),HASH_LENGTH);
                path_hash_iter+=HASH_LENGTH;				
            #endif
            path_iter += (Z*size_for_level);
            temp = temp>>1;		
        }
    }
    else {	
        for(uint8_t i = 0;i<D_lev+1;i++) {
                //printf("i = %d, temp = %d\n",i,temp);
            memcpy(path_iter,inmem_tree_l[level]+((Z*size_for_level)*(temp-1)),(Z*size_for_level));
            
            #ifndef PASSIVE_ADVERSARY
                if(i!=D_lev) {
                    if(temp%2==0){
                        memcpy(path_hash_iter, inmem_hash_l[level]+(HASH_LENGTH*(temp-1)),HASH_LENGTH);
                        path_hash_iter+=HASH_LENGTH;
                        memcpy(path_hash_iter, inmem_hash_l[level]+(HASH_LENGTH*(temp)),HASH_LENGTH);
                        path_hash_iter+=HASH_LENGTH;
                            
                            #ifdef DEBUG_INTEGRITY
                            printf("LS : Level = %d, Bucket no = %d, Hash = ",level, temp);
                            for(uint8_t l = 0;l<HASH_LENGTH;l++)
                                printf("%c",(*(inmem_hash_l[level]+(HASH_LENGTH*(temp-1)) + l) %26)+'A');
                            printf("\nLS : Level = %d, Bucket no = %d, Hash = ",level, temp + 1);
                            for(uint8_t l = 0;l<HASH_LENGTH;l++)
                                printf("%c",(*(inmem_hash_l[level]+(HASH_LENGTH*(temp)) + l) %26)+'A');
                            printf("\n");
                            #endif
                    }
                    else{	
                        memcpy(path_hash_iter, inmem_hash_l[level]+(HASH_LENGTH*(temp-2)),HASH_LENGTH);
                        path_hash_iter+=HASH_LENGTH;
                        memcpy(path_hash_iter, inmem_hash_l[level]+(HASH_LENGTH*(temp-1)),HASH_LENGTH);
                        path_hash_iter+=HASH_LENGTH;
                            
                            #ifdef DEBUG_INTEGRITY
                            printf("LS : Level = %d, Bucket no = %d, Hash = ",level, temp-1);
                            for(uint8_t l = 0;l<HASH_LENGTH;l++)
                                printf("%c",(*(inmem_hash_l[level]+(HASH_LENGTH*(temp-2)) + l) %26)+'A');
                            printf("\nLS : Level = %d, Bucket no = %d, Hash = ", level, temp);
                            for(uint8_t l = 0;l<HASH_LENGTH;l++)
                                printf("%c",(*(inmem_hash_l[level]+(HASH_LENGTH*(temp-1)) + l) %26)+'A');
                            printf("\n");
                            #endif
                    }					
                }
                else{
                    memcpy(path_hash_iter, inmem_hash_l[level]+(HASH_LENGTH*(temp-1)),HASH_LENGTH);
                    path_hash_iter+=(HASH_LENGTH);
                    #ifdef DEBUG_INTEGRITY
                        printf("LS : Level = %d, Bucket no = %d, Hash = ",level, temp);
                        for(uint8_t l = 0;l<HASH_LENGTH;l++)
                            printf("%c",(*(inmem_hash_l[level]+(HASH_LENGTH*(temp-1)) + l) %26)+'A');
                        printf("\n");
                    #endif
                            
                }
            #endif				
            
            
            //DEBUG module				
            /*
            uint32_t* iter_ptr = (uint32_t*) path_iter;

            for(uint8_t q = 0; q<Z; q++){
                uint32_t *block = iter_ptr + q*(size_for_level/4) ;
                printf("IN LS , temp = %d : (%d,%d)\n",temp, *(block+4), *(block+5));
                                        
                //for(uint8_t j = 0 ; j<16;j++){
                //	printf("%d,", *(iter_ptr+6+j));
                //}
                printf("\n");
            }	
            */

            path_iter += (Z*size_for_level);
            temp = temp>>1;		
        }
    }		
	
	return path;
}

void LocalStorage::deleteObject()
{

}
void LocalStorage::copyObject()
{

}

