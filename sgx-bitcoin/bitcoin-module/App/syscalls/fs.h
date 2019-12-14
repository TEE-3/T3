#ifndef __FS_H_
#define __FS_H_

#if defined(__cplusplus)
extern "C" {
#endif

int ocall_open(char* filename, int flags, int mode);
size_t ocall_read(int fd, char* buf, size_t count);
size_t ocall_write(int fd, char* buf, size_t count);
size_t ocall_seek(int fd, size_t offset, size_t start);
int ocall_close(int fd);

#if defined(__cplusplus)
}
#endif


#endif
