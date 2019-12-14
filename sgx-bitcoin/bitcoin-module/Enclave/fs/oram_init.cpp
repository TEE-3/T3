#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "oram.h"
#include "../Enclave.h"
#include "../common/fs_common.h"
#include "../common/memory.h"
#include "Enclave_t.h"

char dummy_data[BLOCK_SIZE];

void populate_data(int fd)
{
  // get oram metadata info for this file
  struct oram_tree_t* otree = &(oramTab[fd].otree);
  size_t count_nodes = (otree->nodecount);
  size_t leaf_nodes = (otree->leafcount);

  // debugging
  debug("name of file: %s\n", oramTab[fd].name);
  debug("otree_start %x, otree_end %x\n", otree->start, otree->end);
  debug("count_nodes %ld, leaf_nodes %ld\n", count_nodes, leaf_nodes);

  // open the file for reading
  int ocall_fd;
  ocall_open(&ocall_fd, oramTab[fd].name, O_RDWR, S_IRUSR);
  assert(ocall_fd >= 0);
  debug("file opened successfully\n");

  // populate the data from the original file
  char data[BLOCK_SIZE];
  size_t ret;
  for (int j = 0; j < 10; j++) {
    debug("Populating: %d\n", j);
    ocall_read(&ret, ocall_fd, (char*) &data, BLOCK_SIZE);
    oram_write(fd, (char*) &data, BLOCK_SIZE);
  }
}

void initialize_pmap(int fd, uint64_t leaf)
{
  // initialize simply at same indexes
  int start = leaf-1;

  // XXX. changed
  oramTab[fd].opmap.pmap_start = (int*) get_trusted_memory(4*leaf); 
  oramTab[fd].opmap.pmap_end = oramTab[fd].opmap.pmap_start + 
                               (((2*leaf)-1) * sizeof(int));

  int* tmp = oramTab[fd].opmap.pmap_start;
  for (int i = 0; i < leaf; i++)
  {
    tmp = (int*) 
          ((size_t) oramTab[fd].opmap.pmap_start + i*(sizeof(int)));

    *tmp = (start+i);
  }

#if CUSTOM_DEBUG == 1
  //debug_pmap(fd);
#endif

}

void initialize_stash(int fd)
{
  // get the handler
  struct oram_stash_t* ostash = &(oramTab[fd].ostash);

  // initialize the stash space
  ostash->size = BLOCK_SIZE * INITIAL_STASH_BLOCKS;
  debug("stash->size: %ld\n", ostash->size );

#ifdef UNTRUSTED_MEMORY
  ostash->start = (void*) get_untrusted_memory(ostash->size);
#else
  ostash->start = (void*) get_trusted_memory(ostash->size);
#endif
  ostash->end = (void*) ((size_t) ostash->start + ostash->size);

#if CUSTOM_DEBUG == 1
  debug("stash_start: %lx, stash_end: %lx, stash_size: %ld\n", 
        ostash->start, ostash->end, ostash->size);
#endif

  // initialize the block id map
  size_t blockmap_memory = INITIAL_STASH_BLOCKS * sizeof(struct oram_block_map_t);
  ostash->s_block_map = (struct oram_block_map_t*) 
                        get_trusted_memory(blockmap_memory);

  struct oram_block_map_t* tmp = ostash->s_block_map;
  size_t count_blocks = ostash->size/BLOCK_SIZE;

  //debug("count_blocks: %ld, size of block_map_t: %d\n", 
  //                      count_blocks, sizeof(struct oram_block_map_t));

  for (int i = 0; i < count_blocks; i++) {
    tmp = (struct oram_block_map_t*) 
          ((size_t) ostash->s_block_map + i*sizeof(struct oram_block_map_t));

    tmp->id = -1;
    tmp->leaf = -1;

  }

#if CUSTOM_DEBUG == 1
  debug("stash initialized successfully\n");
#endif

}

void initialize_tree(int fd, uint64_t leaf)
{
  struct oram_tree_t* otree = &(oramTab[fd].otree);

  otree->size = (((2*leaf) - 1) * NODE_SIZE);

  // Debugging
  debug("Node Size: %ld\n", NODE_SIZE);
  debug("Tree Size: %lld\n", otree->size);

  otree->start = (void*) get_untrusted_memory(otree->size);
  otree->end = (void*) (((size_t) otree->start) + otree->size);

  // save the leaf and node count
  otree->leafcount = leaf;
  otree->nodecount = ((2*leaf)-1);

#ifdef CUSTOM_DEBUG
  debug("tree start: %lx, tree end: %lx, tree size: %ld\n", otree->start,
              otree->end, otree->size);
  debug("leafcount: %d, nodecount: %d\n", otree->leafcount, 
              otree->nodecount);
#endif

  // initialize the t_node_map
  size_t nodemap_memory = (otree->nodecount) * sizeof(struct oram_node_map_t);

  // XXX. changed
  otree->t_node_map = (struct oram_node_map_t*) get_untrusted_memory(nodemap_memory);

  debug("Completed -- creating nodemap\n");

  // initialize the t_node_map
  struct oram_node_map_t* tmp = otree->t_node_map;
  size_t count_nodes = (otree->nodecount);

  for (int i = 0; i < count_nodes; i++) {

    tmp = (struct oram_node_map_t*) ((size_t) otree->t_node_map + i*sizeof(struct oram_node_map_t));

    tmp->node_id = i;
    tmp->leaf = i;

    for (int k = 0; k < BLOCKS_PER_NODE; k++) {
      tmp->block_id[k] = -1;
    }

    // randomly assign one to be the real block
    if (i >= (otree->leafcount-1)) {
      tmp->block_id[0] = (i-(otree->leafcount-1));
    }
  }

  debug("Completed -- initializing nodemap\n");

#ifdef CUSTOM_DEBUG
  debug("tree initialized succesfully\n");
#endif

}
