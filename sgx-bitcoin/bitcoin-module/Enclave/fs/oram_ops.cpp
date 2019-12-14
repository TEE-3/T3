#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "sgx_spinlock.h"

#include "oram.h"
#include "../Enclave.h"
#include "../common/fs_common.h"
#include "../common/memory.h"

#include "user_types.h"

char* dummy = (char*) malloc(BLOCK_SIZE);

void write_path_into_oram(int fd, int leaf)
{

#if ENCLAVE_DEBUG==1
  debug("{START} writing back into oram\n");
#endif

  struct oram_tree_t* otree = &(oramTab[fd].otree);
  void* otree_start = otree->start;
  void* otree_end = otree->end; 
  struct oram_node_map_t* cur_node_map;

  int start = leaf;
  void* node;
  void* stash_free_block;
  size_t offset;

  while (true) 
  {
    // find the memory space where this node is
    offset = start * sizeof(struct oram_node_t);
    node = (void*) ((size_t) otree_start + offset);
    // assert(node <= otree_end);

    // find the current node's map
    offset = start * sizeof(struct oram_node_map_t);
    cur_node_map = (struct oram_node_map_t*) 
                  ((size_t) otree->t_node_map + offset);

#if ENCLAVE_DEBUG == 1
    debug("writing node: %d\n", cur_node_map->node_id);
#endif

    for (int i = 0; i < BLOCKS_PER_NODE; i++) {
      // find the memory location for the block
      void* block = (void*) ((size_t) node + i*BLOCK_SIZE); 

      // find free space in the stash and save it there (updating stashmap also)
      block_from_stash(fd, block, cur_node_map->node_id);
    }

    // breaking condition
    if (start <= 0) break;

    // next node to save into the stash
    start -= 1;
    start /= 2;
  }

#if ENCLAVE_DEBUG == 1
  debug("{FIN} writing back into oram\n");
#endif

}

void process_stash(int fd, char* buf, int blocknum, enum o_rw_type type)
{
#if ENCLAVE_DEBUG == 1
  debug("{START} processing stash\n");
#endif

  struct oram_stash_t* ostash = &(oramTab[fd].ostash);
  struct oram_block_map_t* obmap = ostash->s_block_map;
  char* block_buf;
  int stash_count = ostash->size/BLOCK_SIZE;

  //debug("stash_count: %d\n", ostash->size);

  for (int i = 0; i < stash_count; i++)
  {
    // check the block map
    obmap = (struct oram_block_map_t*)
            ((size_t) ostash->s_block_map + i*sizeof(struct oram_block_map_t));

    // find location for the block buffer in stash
    block_buf = (char*) 
            ((size_t) ostash->start + i*(sizeof(BLOCK_SIZE)));  

    if (obmap->id == blocknum)
    {
#if ENCLAVE_DEBUG == 1
      debug("found required block %d\n", obmap->id);    
#endif
      if (type == READ) 
        memcpy(buf, block_buf, BLOCK_SIZE);
      else 
        memcpy(block_buf, buf, BLOCK_SIZE);
    } 
    else 
    {
      //TODO: do some dummy interactions
      memcpy(dummy, buf, BLOCK_SIZE);
    }
  }

#if ENCLAVE_DEBUG == 1
  debug("{FIN} processing stash\n");
#endif

}

/* 
  function to read the path from the start to the
  leaf into the stash 
*/
void read_path_from_oram(int fd, int leaf)
{

#if ENCLAVE_DEBUG == 1
  debug("{START} reading path from oram\n");
#endif

  struct oram_tree_t* otree = &(oramTab[fd].otree);
  void* otree_start = otree->start;
  void* otree_end = otree->end; 
  struct oram_node_map_t* cur_node_map;

  int start = leaf;
  void* node;
  void* stash_free_block;
  size_t offset;

  while (true) 
  {
    // find the memory space where this node is
    offset = start * sizeof(struct oram_node_t);
    node = (void*) ((size_t) otree_start+offset);
    // assert(node <= otree_end);

    // find the current node's map
    offset = (start)*sizeof(struct oram_node_map_t);
    cur_node_map = (struct oram_node_map_t*) 
                  ((size_t) otree->t_node_map + offset);

#if ENCLAVE_DEBUG == 1
    debug("reading node: %d\n", cur_node_map->node_id);
#endif

    for (int i = 0; i < BLOCKS_PER_NODE; i++) {
      // find the block-id of the block
      int block_id = cur_node_map->block_id[i];

      // find the memory location for the block
      void* block = (void*) ((size_t) node + i*BLOCK_SIZE); 

      // find free space in the stash and save it there (updating stashmap also)
      free_space_stash(fd, block, block_id, leaf);
    }

    // breaking condition
    if (start <= 0) break;

    // next node to save into the stash
    start -= 1;
    start /= 2;
  }

#if ENCLAVE_DEBUG == 1
  debug("{FIN} processing stash\n");
#endif

}

/* function to get the leaf from the pmap */
int get_leaf(int fd, int blocknum)
{
  // tmp -> pmap_start ; tmp_end -> pmap_end
  int* tmp = oramTab[fd].opmap.pmap_start;
  int* tmp_end = oramTab[fd].opmap.pmap_end;

  // leafcount
  int leafcount = oramTab[fd].otree.leafcount;
  int start = 0, ans = 0;

#if ENCLAVE_DEBUG == 1
  debug("leafcount: %d, tmp: %lx, tmp_end: %lx\n", 
      leafcount, tmp, tmp_end);
#endif

  // stream through the whole pmap
  bool flag;
  for(int i = 0; i < leafcount; i++) {
    tmp = (int*)
          ((size_t) oramTab[fd].opmap.pmap_start + i*sizeof(int));

    if (i == blocknum) {
      flag = true;
      ans = *tmp;
    }

    // assert(tmp <= tmp_end);
  }

  assert(flag == true);
  return ans;
}

/* handler to perform both read and write */
bool oram_read_write_handler(int fd, char* buf, int blocknum, 
                             enum o_rw_type t) 
{
#if ENCLAVE_DEBUG == 1
  debug("{START} oram processing\n");
#endif

  // find the leaf required for this block
  int leaf = get_leaf(fd, blocknum);

#if ENCLAVE_DEBUG == 1
  debug("required-block: %d, corresponding-leaf: %d\n", 
         blocknum, leaf);
#endif

#ifdef BTHREAD
  // make sure we are ready 
  // debug("waiting for bthread\n");

  int tmp = 0x100;
  while (tmp != ORAM_BG_READY) {
    tmp = *oram_bg_flag;
  }

  // debug("thread ready\n");
#endif

  // read the whole path to the leaf into the stash
  read_path_from_oram(fd, leaf);

  // do READ/WRITE etc.
  process_stash(fd, buf, blocknum, t);

  // get a random number
  rand();

#ifdef BTHREAD
  // debug("setting variables\n");

  sgx_spin_lock(shared_lock);

  // set up the data structures
  *oram_bg_fd = fd;
  *oram_bg_leaf = leaf;

  // signal the background thread to write-back
  *oram_bg_flag = ORAM_BG_WRITE;

  sgx_spin_unlock(shared_lock);

#else
  // write-back into the oram tree
  write_path_into_oram(fd, leaf);

#if ENCLAVE_DEBUG == 1
  debug("{FIN} oram processing\n");
#endif

#endif

  return true;
}

