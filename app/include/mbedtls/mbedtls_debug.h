#ifndef _MBEDTLS_DEBUG_H_
#define _MBEDTLS_DEBUG_H_

#include "osapi.h"

#define MBEDTLS_SSL_DEBUG_MSG( level, args )            os_printf args;
#define MBEDTLS_SSL_DEBUG_RET( level, ... )             os_printf (__VA_ARGS__);
#define MBEDTLS_SSL_DEBUG_BUF( level, ... )             os_printf (__VA_ARGS__);
#define MBEDTLS_SSL_DEBUG_MPI( level, text, X )         do { } while( 0 )
#define MBEDTLS_SSL_DEBUG_ECP( level, text, X )         do { } while( 0 )
#define MBEDTLS_SSL_DEBUG_CRT( level, text, crt )       do { } while( 0 )

#endif
