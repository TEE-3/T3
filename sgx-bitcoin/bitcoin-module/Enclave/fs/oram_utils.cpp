#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "sgx_trts.h"

#include "oram.h"
#include "../Enclave.h"
#include "../common/fs_common.h"
#include "../common/memory.h"

extern char* dummy;

int round_leaf(int current)
{
  int start = 1;

  while(start < current) {
    start *= 2;
  }

  return start;
}

bool isInPath(int node, int leaf)
{
  if (node == leaf) return true;

  while (node < leaf) {
    node *= 2;
    node += 1;

    if (node == leaf) return true;
  }

  return false;
}

/* write back from the stash */
void block_from_stash(int fd, void* block, int nodeid)
{
  struct oram_stash_t* ostash = &(oramTab[fd].ostash);
  struct oram_block_map_t* blockmap = ostash->s_block_map;
  char* stash_block;
  int stash_count = (ostash->size / BLOCK_SIZE);
  
  for (int i = 0; i < stash_count; i++)
  {
    // get the blockmap stat
    blockmap = (struct oram_block_map_t*)
               ((size_t) ostash->s_block_map + i*sizeof(struct oram_block_map_t));

    // get the actual block
    stash_block = (char*)
            ((size_t) ostash->start + i*(BLOCK_SIZE));

    if (blockmap->id != -1 && isInPath(nodeid, blockmap->leaf))
    {
#ifdef CUSTOM_DEBUG
        debug("%d is in path for block %d having leaf %d\n", 
                nodeid, blockmap->id, blockmap->leaf);
#endif

        // copy the stash_block into the block
        memcpy(block, stash_block, BLOCK_SIZE);
        
        // reset the blockmap id
        blockmap->id = -1;

        // return;
    } 
    else 
    {
      // paste some dummy things

      memcpy(dummy, stash_block, BLOCK_SIZE);
    }

  }
}

/* find free space in stash */
void free_space_stash(int fd, void* block, int block_id, int leaf_id)
{

  struct oram_stash_t* ostash = &(oramTab[fd].ostash);
  void* ostash_start = ostash->start;
  void* ostash_end = ostash->end;
  struct oram_block_map_t* stash_block_map = (ostash->s_block_map);
  int stash_count = (ostash->size/BLOCK_SIZE);
  void* ret;

  for (int i = 0; i < stash_count; i++)
  { 
    stash_block_map = (struct oram_block_map_t*) 
                      ((size_t) ostash->s_block_map + i*sizeof(struct oram_block_map_t));

    if (stash_block_map->id == -1) {

      // add the offset
      ret = (void*) ((size_t) ostash_start + (i*BLOCK_SIZE));
      assert(ret < ostash_end);

      // copy the block
      memcpy(ret, block, BLOCK_SIZE);

      // update the map to store block-id and leaf
      stash_block_map->id = block_id;
      stash_block_map->leaf = leaf_id;

      return;

    } else {
      memcpy(dummy, block, BLOCK_SIZE);
    }

  }

  // shouldn't come here, means no space in stash
#ifdef CUSTOM_DEBUG
  debug("No more space in the stash\n");
#endif

  assert(false);
}

int rand() {
  size_t len = 4;
  unsigned char* num = (unsigned char*) malloc(len);

  // read a random number
  sgx_read_rand(num, len);

  return *num;
}

