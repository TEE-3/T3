#ifndef __MEMORY_H_
#define __MEMORY_H_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* memory operations */
void* get_trusted_memory(size_t size);
int   free_trusted_memory(void* ptr);

void* get_untrusted_memory(long long size);
int   free_untrusted_memory(void* ptr);


#if defined(__cplusplus)
}
#endif

#endif
