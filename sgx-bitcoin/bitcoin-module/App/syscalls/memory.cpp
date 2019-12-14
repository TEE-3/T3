#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#include "sgx_urts.h"
#include "../Enclave_u.h"

#include "memory.h"
#include "../App.h"

void* ocall_get_untrusted_memory(long long size)
{
  void* ret;
  if (size < 0) {
    // XXX. check this out
    long long int size2 = 3221225472;
    ret = malloc(size2);
  } else {
    debug("untrusted memory requested: %lld\n", size);
    ret = malloc(size);
  }

  if (ret == NULL) {
    debug("malloc returned null\n");
  }

  assert(ret != NULL);

  debug("returning\n");

  return ret;
}

void ocall_free_untrusted_memory(void* ptr)
{
  free(ptr);
}
