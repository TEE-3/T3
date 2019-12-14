#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
// #include <sys/mman.h>

#include "../Enclave_t.h"
#include "../Enclave.h"

#include "memory.h"

void* get_trusted_memory(size_t size)
{
//#ifdef CUSTOM_DEBUG
  debug("Requested trusted memory: %ld\n", size); 
//#endif

  // simple sanity check
  // assert(size <= 100000);

  void* ret = (void*) malloc(size);

  if (ret == NULL) {
    debug("malloc returned null\n");
  }

  assert(ret != NULL);

  return ret;
}

int free_trusted_memory(void* ptr)
{
  free(ptr);
  // TODO: add some assertion

  return 1;
}

void* get_untrusted_memory(long long size)
{
#ifdef CUSTOM_DEBUG
  debug("size of uint64_t: %lld\n", sizeof(uint64_t));
  debug("Requested untrusted memory: %lld\n", size);
#endif

  if (size < 0)
    size = 3221225472;

  void* ret;
  ocall_get_untrusted_memory(&ret, size);
  assert(ret != NULL);

  return ret;
}

int free_untrusted_memory(void* ptr)
{
  ocall_free_untrusted_memory(ptr);

  // TODO: add some assertion 
  return 1;
}
