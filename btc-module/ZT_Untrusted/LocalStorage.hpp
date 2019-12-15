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
LocalStorage.hpp
*/

#define HASH_LENGTH 32
#define FILESTREAM_MODE 1
#define DEBUG_LS 1
// #define DEBUG_INTEGRITY 1
// Utilization Parameter is the number of blocks of a bucket that is filled at start state. ( 4 = MAX_OCCUPANCY )
#define UTILIZATION_PARAMETER 4
#define ADDITIONAL_METADATA_SIZE 24
//#define NO_CACHING 1
//#define CACHE_UPPER 1
//#define PASSIVE_ADVERSARY 1
//#define PRINT_BUCKETS 1

class LocalStorage
{
	uint32_t dataSize;
	uint32_t Z;
	uint32_t D;

	//Take this value as input parameter !
	bool inmem;
	unsigned char* inmem_tree;
	unsigned char* inmem_hash;
	unsigned char** inmem_tree_l;
	unsigned char** inmem_hash_l;
	uint64_t datatree_size;
	uint64_t hashtree_size;
	uint32_t bucket_size;
	uint32_t recursionBlockSize;
	int32_t recursion_levels=0;
	uint32_t objectkeylimit;

public:
	LocalStorage();
	LocalStorage(LocalStorage &ls);

	void connect();
	void fetchHash(uint32_t objectKey, unsigned char* hash_buffer, uint32_t hashsize, uint32_t recursion_level);
	uint8_t uploadObject(unsigned char *serialized_bucket, uint32_t objectKey, unsigned char* hash, uint32_t hashsize, uint32_t size_for_level, uint32_t recursion_level);
	uint8_t uploadPath(unsigned char *serialized_path, uint32_t leafLabel, unsigned char *path_hash,uint32_t level, uint32_t D_level);
	unsigned char* downloadPath(unsigned char* data, uint32_t leafLabel, unsigned char *path_hash, uint32_t path_hash_size, uint32_t level, uint32_t D);
	void setParams(uint32_t maxBlocks, uint32_t D, uint32_t Z, uint32_t stashSize, uint32_t dataSize, bool inmem, uint32_t recursion_block_size, int8_t recursion_levels);
	void deleteObject();
	void copyObject();
};
