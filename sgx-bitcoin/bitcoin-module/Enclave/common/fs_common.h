#ifndef __FS_COMMON_H_
#define __FS_COMMON_H_

#define MAX_FILES           20
#define MAX_FILENAME        100
#define STARTING_FILE_SIZE (16 * 1024 * 1024)

/* File Definitions */
#define O_RDONLY  0x0000    /* open for reading only */
#define O_WRONLY  0x0001    /* open for writing only */
#define O_RDWR    0x0002    /* open for reading and writing */
#define O_CREAT   0x0200    /* create if nonexistant */
#define O_TRUNC   0x0400    /* truncate to zero length */
#define O_EXCL    0x0800    /* error if already exists */

/* Seek Definitions */
#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2

/* S_I.. Definitions */
#define S_IRWXU 0000700     /* RWX mask for owner */
#define S_IRUSR 0000400     /* R for owner */
#define S_IWUSR 0000200     /* W for owner */
#define S_IXUSR 0000100     /* X for owner */

/* file handler for inmemory fs */
struct file_handle_t {
  int fd;             // file descriptor
  char* name;         // name of the file
  size_t size;        // size of the file/buffer
  size_t offset;      // current file offset
  size_t usedbuf;     // amount of the buffer which is used
  int nonempty;       // mark as non-empty or empty
  void* filebuf;      // pointer to the file buffer
};

extern struct file_handle_t fileTab[MAX_FILES];

#if defined(__cplusplus)
extern "C" {
#endif

/* file handler operations */
int get_new_file_handle(char* filename, size_t file_size);
int clear_file_handle(int fd);
size_t update_file_offset(int fd, size_t count);
size_t update_file_buffer(int fd);
void debug_file_stats(int fd);

#if defined(__cplusplus)
}
#endif



#endif
