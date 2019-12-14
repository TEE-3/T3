#ifndef __ORAM_H_
#define __ORAM_H_

#include "sgx_spinlock.h"

#include "../common/fs_common.h"

#include <stdint.h>

/* oram tree definitions */
#ifdef CUSTOM_BLOCK_PER_NODE
  #define BLOCKS_PER_NODE       CUSTOM_BLOCK_PER_NODE
#else
  #define BLOCKS_PER_NODE       1
#endif

#ifdef CUSTOM_BLOCK_SIZE
  #define BLOCK_SIZE            CUSTOM_BLOCK_SIZE
#else
  #define BLOCK_SIZE            4096
#endif

#ifdef CUSTOM_INITIAL_LEAF
  #define INITIAL_LEAF          CUSTOM_INITIAL_LEAF
#else
  #define INITIAL_LEAF          4
#endif

#define NODE_SIZE               (BLOCKS_PER_NODE * BLOCK_SIZE)
#define MAX_ORAM_LEAF           1024

/* stash definitions */
#define INITIAL_STASH_BLOCKS    20

#ifdef ENCLAVE_DEBUG
#define ENCLAVE_DEBUG ENCLAVE_DEBUG
#else
#define ENCLAVE_DEBUG 0
#endif

/* dummy struct for an oram block */
struct oram_block_t {
  char data[BLOCK_SIZE];
};

struct oram_block_map_t {
  int id;
  int leaf;
};

struct oram_node_map_t {
  int node_id;
  int leaf;
  int block_id[BLOCKS_PER_NODE];
};

/* dummy struct for an oram node */
struct oram_node_t {
  struct oram_block_t blocks[BLOCKS_PER_NODE];
};

struct oram_tree_t {
  /* oram tree specific */

  void* start;
  void* end;
  uint64_t size;

  /* misc details */

  uint64_t leafcount;                          // count of the leaf within the oram tree
  uint64_t nodecount;                          // count of nodes within the oram tree
  struct oram_node_map_t* t_node_map;          // maintains the id of all blocks within each node
};

struct oram_stash_t {
  /* oram stash specific */
  void*   start;
  void*   end;
  size_t  size;

  struct oram_block_map_t* s_block_map;
};


/* oram pmap specific */
struct oram_pmap_t {
  int* pmap_start;
  int* pmap_end;
  int filled_entries;
};

struct oram_file_handle_t {

  /* generic */
  int fd;                           // file descriptor
  char* name;                       // name of the file
  size_t offset;                    // current file offset
  int nonempty;                     // mark as non-empty or empty (for write-back purposes)
  void* filebuf;                    // pointer to the file buffer
  struct oram_tree_t    otree;      // information about the oram tree
  struct oram_stash_t   ostash;     // information about the oram stash
  struct oram_pmap_t    opmap;      // information about the oram pmap 

};

enum o_rw_type{ READ, WRITE };

extern struct oram_file_handle_t oramTab[MAX_FILES];

/* in oram_bg_thread.cpp */
extern int* oram_bg_flag;
extern int* oram_bg_fd;
extern int* oram_bg_leaf;
extern sgx_spinlock_t* shared_lock;

#if defined(__cplusplus)
extern "C" {
#endif

/* in oram_handle.cpp */
int get_new_oram_file_handle(char* filename, size_t filesize);

/* in oram_utils.cpp */
int round_leaf(int current);
void free_space_stash(int fd, void* block, int block_id, int leaf);
void block_from_stash(int fd, void* block, int node_id);
int rand(void);

/* in oram_ops.cpp */
bool oram_read_write_handler(int fd, char* buf, int blocknum, 
                             enum o_rw_type t);
void write_path_into_oram(int fd, int leaf);

/* in oram_init.cpp */
void initialize_pmap(int fd, uint64_t leaf);
void initialize_stash(int fd);
void initialize_tree(int fd, uint64_t leaf);

// XXX. fix this
void populate_data(int fd);

/* in oram_debug.cpp */
void debug_tree_map(int fd);
void debug_stash_map(int fd);
void debug_pmap(int fd);

/* in oram_fs.cpp */
size_t oram_write(int fd, char* buf, size_t count);

#if defined(__cplusplus)
}
#endif

#endif
