#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <cstring>

#include "oram.h"
#include "../common/memory.h"
#include "../common/fs_common.h"
#include "../Enclave.h"

#define CUSTOM_DEBUG

struct oram_file_handle_t oramTab[MAX_FILES];

int get_new_oram_file_handle(char* filename, size_t filesize)
{
  int i = -1;
  for (i = 0; i < MAX_FILES; i++)
  {
    if (oramTab[i].fd == 0)
      break;
  }
  assert(i >= 0 && i < MAX_FILES);

#ifdef CUSTOM_DEBUG
  debug("Obtained index %d in oramTab\n", i);
#endif

  struct oram_file_handle_t* ohandle = &oramTab[i];

  // save the file descriptor
  ohandle->fd = i;

  // save the file name
  ohandle->name = (char*) get_trusted_memory(MAX_FILENAME);
  assert(strlen(filename) <= MAX_FILENAME);
  memcpy(ohandle->name, filename, strlen(filename));

  // start the offset at 0
  ohandle->offset = 0;

  // find out the oram buffer size & allocate enough buffer space
  int leaf;
  if (filesize == 0) { 
#ifdef CUSTOM_DEBUG
    debug("initializing empty oram tree (leaf = %d)\n", INITIAL_LEAF);
#endif
    leaf = INITIAL_LEAF;
  } else {

    leaf = round_leaf( (filesize / BLOCK_SIZE) );
#ifdef CUSTOM_DEBUG
    debug("initializing non-empty oram tree (leaf = %d) \n", leaf);
#endif
  }

  // initialize the position map & the stash
  initialize_tree(i, leaf);
  initialize_stash(i);
  initialize_pmap(i, leaf);

#ifdef CUSTOM_DEBUG
  debug("Successfully initialized the position map & stash\n");
#endif

  return i;
}

