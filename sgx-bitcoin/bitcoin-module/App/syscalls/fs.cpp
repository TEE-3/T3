#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "sgx_urts.h"
#include "../Enclave_u.h"
#include "../App.h"

int ocall_open(char* filename, int flags, int mode)
{
#ifdef CUSTOM_DEBUG
  debug("ocall_open\n");
  debug("filename: %s, flags: %d, mode: %d\n", filename, flags, mode);
#endif
  int ret = open(filename, flags, mode);
  return ret;
}

size_t ocall_read(int fd, char* buf, size_t count)
{
#ifdef CUSTOM_DEBUG
  debug("ocall_read\n");
  debug("fd: %d, count: %ld\n", fd, count);
#endif
  return read(fd, buf, count);
}

size_t ocall_write(int fd, char* buf, size_t count)
{
  debug("ocall_write: fd: %d, count: %ld\n", fd, count);
  return write(fd, buf, count);
}

size_t ocall_lseek(int fd, size_t offset, int whence)
{
  return lseek(fd, offset, whence);
}

size_t ocall_check_file_size(char* filename)
{
  debug("ocall_check_file_size, filename: %s\n", filename);

  struct stat st;
  stat(filename, &st);

  if (st.st_size <= 0) {
    printf("file is empty file\n");
  } else {
    // try to open the file
    if (!fopen(filename, "r"))
      return 0;
  }

  return st.st_size;
}

int ocall_close(int fd)
{
  return close(fd);
}
