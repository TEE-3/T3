#ifndef __MEMORY_H__
#define __MEMORY_H__

#if defined (__cplusplus)
extern "C" {
#endif

void* ocall_get_untrusted_memory(long long size);
void ocall_free_untrusted_memory(void* ptr);

#if defined (__cplusplus)
}
#endif

#endif
