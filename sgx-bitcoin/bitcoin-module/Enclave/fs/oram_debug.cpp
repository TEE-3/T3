#include <stdio.h>

#include "oram.h"
#include "../Enclave.h"

void debug_stash_map(int fd)
{
  debug("[DEBUG] Stash Block Map for fd => %d\n", fd);

  struct oram_block_map_t* blockmap = 
                          oramTab[fd].ostash.s_block_map;
  int stash_count = (oramTab[fd].ostash.size / BLOCK_SIZE);

  for (int i = 0; i < stash_count; i++)
  {
    blockmap =  (struct oram_block_map_t*) 
            ((size_t) oramTab[fd].ostash.s_block_map + i*sizeof(struct oram_block_map_t));
    debug("stash-block-id: %d\n", blockmap->id);
  }

  debug("[DEBUG] END\n");
}

void debug_tree_map(int fd)
{

  debug("[DEBUG] Tree Node Map for fd => %d\n", fd);

  struct oram_node_map_t* nodemap = 
                oramTab[fd].otree.t_node_map;
  int count_nodes = oramTab[fd].otree.nodecount;
  int nodeid, blockid;

  for (int i = 0; i < count_nodes; i++)
  {
    nodemap = (struct oram_node_map_t*) 
          ((size_t) oramTab[fd].otree.t_node_map + i*sizeof(struct oram_node_map_t));

    debug("node-map-address: %lx\n", 
            oramTab[fd].otree.t_node_map + i*sizeof(struct oram_node_map_t));
  
    nodeid = nodemap->node_id;

    debug("node-id: %d\n", nodeid);
    for (int k = 0; k < BLOCKS_PER_NODE; k++)
    {
      blockid = nodemap->block_id[k];
      debug("block-id: %d\n", blockid);
    }

  }

  debug("[DEBUG] End\n");
  
}

void debug_pmap(int fd)
{
  struct oram_pmap_t* opmap = &(oramTab[fd].opmap);
  int* start = (int*) opmap->pmap_start;
  void* end = opmap->pmap_end;
  int leafcount = oramTab[fd].otree.leafcount;

  for (int i = 0; i < leafcount; i++)
  {
    start = (int*) 
            ((size_t)opmap->pmap_start + i*sizeof(int));
    assert(start < end);

    debug("index: %d, position: %d\n", i, *start);
  }

}
