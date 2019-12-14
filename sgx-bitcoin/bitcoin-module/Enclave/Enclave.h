#ifndef _ENCLAVE_H_
#define _ENCLAVE_H_

#include <stdlib.h>
#include <assert.h>

#if defined(__cplusplus)
extern "C" {
#endif

void printf(const char *fmt, ...);

#if defined(__cplusplus)
}
#endif

#define debug(...) \
        do { printf("[ENC] [%s():%d]: ", \
                     __func__, __LINE__); \
             printf(__VA_ARGS__); } while (0)

#endif /* !_ENCLAVE_H_ */
