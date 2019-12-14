#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <cstring>

#include "memory.h"
#include "fs_common.h"
#include "../Enclave.h"

struct file_handle_t fileTab[MAX_FILES];

int get_new_file_handle(char* filename, size_t file_size)
{
  int i = -1;
  for (i = 0; i < MAX_FILES; i++)
  {
    if (fileTab[i].fd == 0)
      break;
  }
  assert(i >= 0 && i < MAX_FILES);

#ifdef CUSTOM_DEBUG
  debug("Obtained index %d in fileTab\n", i);
#endif

  fileTab[i].fd = i;

  fileTab[i].name = (char*) get_trusted_memory(MAX_FILENAME);
  assert(strlen(filename) <= MAX_FILENAME);
  memcpy(fileTab[i].name, filename, strlen(filename));

#ifdef UNTRUSTED_MEMORY
  fileTab[i].filebuf = (char*) get_untrusted_memory(file_size);
#else
  fileTab[i].filebuf = (char*) get_trusted_memory(file_size);
#endif

  fileTab[i].size = file_size;
  fileTab[i].offset = 0;

  // TODO: check if this is accurate
  if (file_size == STARTING_FILE_SIZE)
    fileTab[i].usedbuf = 0;
  else
    fileTab[i].usedbuf = file_size;

  return i;
}

int clear_file_handle(int fd)
{
  assert(fileTab[fd].size != 0);

  fileTab[fd].fd = 0;
  fileTab[fd].size = 0;
  fileTab[fd].offset = 0;
  fileTab[fd].usedbuf = 0;
  fileTab[fd].nonempty = 0; 

#ifdef UNTRUSTED_MEMORY
  free_untrusted_memory(fileTab[fd].filebuf);
#else
  free_trusted_memory(fileTab[fd].filebuf);
#endif

  free_trusted_memory(fileTab[fd].name);

  return 1;
}

size_t update_file_offset(int fd, size_t count)
{
  // assert(fileTab[fd].size != 0);

  fileTab[fd].offset += count;

  if (fileTab[fd].offset > fileTab[fd].size)
    fileTab[fd].size = fileTab[fd].offset;

  return fileTab[fd].offset;
}

size_t update_file_buffer(int fd)
{
  // assert(fileTab[fd].size != 0);

  size_t cur_file_size = fileTab[fd].size;
  size_t fut_file_size = cur_file_size * 2;

  // get the current filebuffer
  char * filebuf = (char*) fileTab[fd].filebuf;

  // get new file buffer
  char* newfilebuf;
#ifdef UNTRUSTED_MEMORY
  newfilebuf = (char*) get_untrusted_memory(fut_file_size);
#else
  newfilebuf = (char*) get_trusted_memory(fut_file_size);
#endif  
  assert(newfilebuf != NULL);

  // copy everything to new file buffer
  memcpy(newfilebuf, filebuf, cur_file_size);

  // update pointers + filesize
  fileTab[fd].filebuf = newfilebuf;
  fileTab[fd].size = fut_file_size;

  // free old buffer
#ifdef UNTRUSTED_MEMORY
  free_untrusted_memory(filebuf);
#else
  free_trusted_memory(filebuf);
#endif

  return fut_file_size;
}

void debug_file_stats(int fd)
{
#ifdef CUSTOM_DEBUG
  debug("[FILE STATS]: \n");
  debug("descriptor: %d\n", fileTab[fd].fd);
  debug("size      : %ld\n", fileTab[fd].size);
  debug("offset    : %ld\n", fileTab[fd].offset);
  debug("used      : %ld\n", fileTab[fd].usedbuf);
#endif
}
