#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */

#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */

/* custom includes */
#include "fs/oram.h"
#include "common/fs_common.h"
#include "common/memory.h"

void printf(const char *fmt, ...)
{
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
}


void entry_point(void)
{
  printf("Successfully entered the SGX enclave\n");

  return;
}

