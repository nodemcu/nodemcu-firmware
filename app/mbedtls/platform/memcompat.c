#include <stdlib.h>
void *mbedtls_calloc_wrap(size_t n, size_t sz) { return calloc(n, sz); }
void mbedtls_free_wrap(void *p) { free(p); }
