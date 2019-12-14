#include <stdio.h>
#include <stdlib.h>

#include "oram.h"
#include "../Enclave.h"
#include "../Enclave_t.h"
#include "../common/fs_common.h"
#include "../common/memory.h"

int oram_open(char* filename, int flags, int mode)
{
  debug("OPEN, parameters: name: %s, flags: %d, mode: %d\n", filename, flags, mode);

  int ret = -1;
  size_t size_oram_in_file;

  debug("[1] checking file size\n");

  // check the file size
  size_t filesize;
  ocall_check_file_size(&filesize, filename);

  debug("Completed -- filename: %s, filesize: %ld\n", filename, filesize); 

  debug("[2] obtaining new file handle\n");

  // get a file handle based on whatever size we have
  ret = get_new_oram_file_handle(filename, filesize); 

  // copy the file into the oram blocks if !empty
  if (filesize != 0) {

#ifdef CUSTOM_DEBUG
    debug("copying the blocks of the (non-empty) file into the oram\n");
#endif

    // TODO: copy the blocks of the file
    // hmm, should i do owrite here? 

    oramTab[ret].nonempty = 1;
  }

  return ret;
}

size_t oram_read(int fd, char* buf, size_t count)
{
#ifdef CUSTOM_DEBUG
  debug("Read %ld bytes from file %d\n", count, fd); 
#endif

  // sanity check
  // assert(oramTab[fd].otree.size != 0);

  // check how many blocks are required and from what offset
  struct oram_file_handle_t* ohandle = &(oramTab[fd]);

  size_t cur_offset = ohandle->offset;
  int starting_block = cur_offset/BLOCK_SIZE;
  int ending_block = (cur_offset+count-1)/BLOCK_SIZE;

#ifdef CUSTOM_DEBUG
  debug("starting_block: %d, ending_block: %d\n", starting_block, 
          ending_block);
#endif

  // do the oram-processing and get each required block
  char* tmp = buf;
  bool ans;
  for (int i = starting_block; i <= ending_block; i++)
  {
    ans = oram_read_write_handler(fd, tmp, i, READ);
    assert(ans == true);
    tmp += BLOCK_SIZE;
  }

  // update the offset
  ohandle->offset += count;

  return 1;
}

size_t oram_write(int fd, char* buf, size_t count)
{
  // sanity check
  // assert(oramTab[fd].otree.size != 0);

#ifdef CUSTOM_DEBUG
  debug("Write %ld bytes into file %d\n", count, fd); 
#endif

  // check how many blocks are required and from what offset
  struct oram_file_handle_t* ohandle = &(oramTab[fd]);

  size_t cur_offset = ohandle->offset;
  int starting_block = cur_offset/BLOCK_SIZE;
  int ending_block = (cur_offset+count-1)/BLOCK_SIZE;
  
#ifdef CUSTOM_DEBUG
  debug("starting_block: %d, ending_block: %d\n", starting_block, 
          ending_block);
#endif

  // do the oram-processing and write each block as required

  char* tmp = buf;
  bool ans;
  for (int i = starting_block; i <= ending_block; i++)
  {
    ans = oram_read_write_handler(fd, tmp, i, WRITE);
    assert(ans == true);
    tmp += BLOCK_SIZE;
  }

  // update the offset
  ohandle->offset += count;

  return 1;
}

size_t oram_lseek(int fd, size_t offset, int whence)
{
  // sanity check
  // assert(oramTab[fd].otree.size != 0);

  size_t ret;
  if (whence == SEEK_SET)
  {

#ifdef CUSTOM_DEBUG
    debug("SEEK_SET to %ld\n", offset);
#endif
    
    oramTab[fd].offset = offset;
    ret = offset;
  } 
  else if (whence == SEEK_CUR)
  {
#ifdef CUSTOM_DEBUG
    debug("SEEK_CUR to %ld\n", offset);
#endif  

    oramTab[fd].offset += offset;
    ret = oramTab[fd].offset;
  }
  else if (whence == SEEK_END)
  {
#ifdef CUSTOM_DEBUG
    debug("SEEK_END to %ld\n", offset);
    debug("{Warning} Not implemented yet..\n");
#endif

  }

  return ret;
}

int oram_close(int fd)
{
  // sanity check
  assert(oramTab[fd].otree.size != 0);

  return 0;
}
