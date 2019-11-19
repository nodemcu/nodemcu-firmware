#include <stdlib.h>

void *mbedtls_calloc(size_t n, size_t sz) { return calloc(n, sz); }
void mbedtls_free(void *p) { free(p); }
