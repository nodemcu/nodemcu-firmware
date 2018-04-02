#include "c_types.h"

#include "user_config.h"

#undef MBEDTLS_HAVE_ASM
#undef MBEDTLS_HAVE_SSE2

// These are disabled until we have a real, working RTC-based gettimeofday
#undef MBEDTLS_HAVE_TIME
#undef MBEDTLS_HAVE_TIME_DATE

#define MBEDTLS_PLATFORM_MEMORY
#undef MBEDTLS_PLATFORM_NO_STD_FUNCTIONS

#undef MBEDTLS_PLATFORM_EXIT_ALT
#undef MBEDTLS_PLATFORM_TIME_ALT
#undef MBEDTLS_PLATFORM_FPRINTF_ALT
#undef MBEDTLS_PLATFORM_PRINTF_ALT
#undef MBEDTLS_PLATFORM_SNPRINTF_ALT
#undef MBEDTLS_PLATFORM_NV_SEED_ALT

#undef MBEDTLS_DEPRECATED_WARNING
#define MBEDTLS_DEPRECATED_REMOVED

#undef MBEDTLS_TIMING_ALT
#undef MBEDTLS_AES_ALT
#undef MBEDTLS_ARC4_ALT
#undef MBEDTLS_BLOWFISH_ALT
#undef MBEDTLS_CAMELLIA_ALT
#undef MBEDTLS_DES_ALT
#undef MBEDTLS_XTEA_ALT
#undef MBEDTLS_MD2_ALT
#undef MBEDTLS_MD4_ALT
#undef MBEDTLS_MD5_ALT
#undef MBEDTLS_RIPEMD160_ALT
#undef MBEDTLS_SHA1_ALT
#undef MBEDTLS_SHA256_ALT
#undef MBEDTLS_SHA512_ALT

#undef MBEDTLS_MD2_PROCESS_ALT
#undef MBEDTLS_MD4_PROCESS_ALT
#undef MBEDTLS_MD5_PROCESS_ALT
#undef MBEDTLS_RIPEMD160_PROCESS_ALT
#undef MBEDTLS_SHA1_PROCESS_ALT
#undef MBEDTLS_SHA256_PROCESS_ALT
#undef MBEDTLS_SHA512_PROCESS_ALT

#undef MBEDTLS_DES_SETKEY_ALT
#undef MBEDTLS_DES_CRYPT_ECB_ALT
#undef MBEDTLS_DES3_CRYPT_ECB_ALT
#undef MBEDTLS_AES_SETKEY_ENC_ALT
#undef MBEDTLS_AES_SETKEY_DEC_ALT
#undef MBEDTLS_AES_ENCRYPT_ALT
#undef MBEDTLS_AES_DECRYPT_ALT

#undef MBEDTLS_TEST_NULL_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT

#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_CAMELLIA_SMALL_MEMORY

#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_MODE_CFB
#define MBEDTLS_CIPHER_MODE_CTR

#undef MBEDTLS_CIPHER_NULL_CIPHER

#define MBEDTLS_CIPHER_PADDING_PKCS7
#define MBEDTLS_CIPHER_PADDING_ONE_AND_ZEROS
#define MBEDTLS_CIPHER_PADDING_ZEROS_AND_LEN
#define MBEDTLS_CIPHER_PADDING_ZEROS

#undef MBEDTLS_ENABLE_WEAK_CIPHERSUITES
#define MBEDTLS_REMOVE_ARC4_CIPHERSUITES

#define MBEDTLS_ECP_DP_SECP192R1_ENABLED
#define MBEDTLS_ECP_DP_SECP224R1_ENABLED
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED
#define MBEDTLS_ECP_DP_SECP192K1_ENABLED
#define MBEDTLS_ECP_DP_SECP224K1_ENABLED
#define MBEDTLS_ECP_DP_SECP256K1_ENABLED
#define MBEDTLS_ECP_DP_BP256R1_ENABLED
#define MBEDTLS_ECP_DP_BP384R1_ENABLED
#define MBEDTLS_ECP_DP_BP512R1_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED

#define MBEDTLS_ECP_NIST_OPTIM

#undef MBEDTLS_ECDSA_DETERMINISTIC

#undef MBEDTLS_KEY_EXCHANGE_PSK_ENABLED
#undef MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED
#undef MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED
#undef MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED

#define MBEDTLS_KEY_EXCHANGE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#undef MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#undef MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED

#undef MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED

#define MBEDTLS_PK_PARSE_EC_EXTENDED

#undef MBEDTLS_ERROR_STRERROR_DUMMY

#define MBEDTLS_GENPRIME

#undef MBEDTLS_FS_IO

#undef MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_FORCE_SHA256
#undef MBEDTLS_ENTROPY_NV_SEED

#undef MBEDTLS_MEMORY_DEBUG
#undef MBEDTLS_MEMORY_BACKTRACE

#define MBEDTLS_PK_RSA_ALT_SUPPORT
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21

#undef MBEDTLS_RSA_NO_CRT
#undef MBEDTLS_SELF_TEST

#define MBEDTLS_SHA256_SMALLER

#define MBEDTLS_SSL_ALL_ALERT_MESSAGES
#undef MBEDTLS_SSL_DEBUG_ALL

#define MBEDTLS_SSL_ENCRYPT_THEN_MAC
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#define MBEDTLS_SSL_FALLBACK_SCSV

#undef MBEDTLS_SSL_HW_RECORD_ACCEL

#define MBEDTLS_SSL_CBC_RECORD_SPLITTING
#define MBEDTLS_SSL_RENEGOTIATION

#undef MBEDTLS_SSL_SRV_SUPPORT_SSLV2_CLIENT_HELLO
#undef MBEDTLS_SSL_SRV_RESPECT_CLIENT_PREFERENCE

#define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH

#undef MBEDTLS_SSL_PROTO_SSL3
#define MBEDTLS_SSL_PROTO_TLS1
#define MBEDTLS_SSL_PROTO_TLS1_1
#define MBEDTLS_SSL_PROTO_TLS1_2
#undef MBEDTLS_SSL_PROTO_DTLS

#define MBEDTLS_SSL_ALPN
#undef MBEDTLS_SSL_DTLS_ANTI_REPLAY
#undef MBEDTLS_SSL_DTLS_HELLO_VERIFY
#undef MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE
#undef MBEDTLS_SSL_DTLS_BADMAC_LIMIT
#define MBEDTLS_SSL_SESSION_TICKETS
#define MBEDTLS_SSL_EXPORT_KEYS
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_TRUNCATED_HMAC

#undef MBEDTLS_THREADING_ALT
#undef MBEDTLS_THREADING_PTHREAD

#define MBEDTLS_VERSION_FEATURES

#undef MBEDTLS_X509_ALLOW_EXTENSIONS_NON_V3
#undef MBEDTLS_X509_ALLOW_UNSUPPORTED_CRITICAL_EXTENSION

#define MBEDTLS_X509_CHECK_KEY_USAGE
#define MBEDTLS_X509_CHECK_EXTENDED_KEY_USAGE
#define MBEDTLS_X509_RSASSA_PSS_SUPPORT

#undef MBEDTLS_ZLIB_SUPPORT

#undef MBEDTLS_AESNI_C
#define MBEDTLS_AES_C
#define MBEDTLS_ARC4_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_BLOWFISH_C
#define MBEDTLS_CAMELLIA_C
#define MBEDTLS_CCM_C
#undef MBEDTLS_CERTS_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CMAC_C
#define MBEDTLS_CTR_DRBG_C
#undef MBEDTLS_DEBUG_C
#define MBEDTLS_DES_C
#define MBEDTLS_DHM_C
#define MBEDTLS_ECDH_C
#undef MBEDTLS_ECDSA_C
#undef MBEDTLS_ECJPAKE_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_ERROR_C
#define MBEDTLS_GCM_C
#undef MBEDTLS_HAVEGE_C
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_MD_C
#undef MBEDTLS_MD2_C
#undef MBEDTLS_MD4_C
#define MBEDTLS_MD5_C
#undef MBEDTLS_MEMORY_BUFFER_ALLOC_C
#define MBEDTLS_NET_C
#define MBEDTLS_OID_C
#undef MBEDTLS_PADLOCK_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PEM_WRITE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_WRITE_C
#define MBEDTLS_PKCS5_C
#undef MBEDTLS_PKCS11_C
#define MBEDTLS_PKCS12_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_RIPEMD160_C
#define MBEDTLS_RSA_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_SSL_CACHE_C
#define MBEDTLS_SSL_COOKIE_C
#define MBEDTLS_SSL_TICKET_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_SRV_C
#define MBEDTLS_SSL_TLS_C
#undef MBEDTLS_THREADING_C
#undef MBEDTLS_TIMING_C
#define MBEDTLS_VERSION_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_CRL_PARSE_C
#define MBEDTLS_X509_CSR_PARSE_C
#define MBEDTLS_X509_CREATE_C
#define MBEDTLS_X509_CRT_WRITE_C
#define MBEDTLS_X509_CSR_WRITE_C
#define MBEDTLS_XTEA_C

#define MBEDTLS_MPI_WINDOW_SIZE            1 /**< Maximum windows size used. */
#define MBEDTLS_MPI_MAX_SIZE             512 /**< Maximum number of bytes for usable MPIs. */

//#define MBEDTLS_CTR_DRBG_ENTROPY_LEN               48 /**< Amount of entropy used per seed by default (48 with SHA-512, 32 with SHA-256) */
#define MBEDTLS_CTR_DRBG_RESEED_INTERVAL         1000 /**< Interval before reseed is performed by default */
//#define MBEDTLS_CTR_DRBG_MAX_INPUT                256 /**< Maximum number of additional input bytes */
//#define MBEDTLS_CTR_DRBG_MAX_REQUEST             1024 /**< Maximum number of requested bytes per call */
//#define MBEDTLS_CTR_DRBG_MAX_SEED_INPUT           384 /**< Maximum size of (re)seed buffer */

#define MBEDTLS_HMAC_DRBG_RESEED_INTERVAL    1000 /**< Interval before reseed is performed by default */
//#define MBEDTLS_HMAC_DRBG_MAX_INPUT           256 /**< Maximum number of additional input bytes */
//#define MBEDTLS_HMAC_DRBG_MAX_REQUEST        1024 /**< Maximum number of requested bytes per call */
//#define MBEDTLS_HMAC_DRBG_MAX_SEED_INPUT      384 /**< Maximum size of (re)seed buffer */

//#define MBEDTLS_ECP_MAX_BITS             521 /**< Maximum bit size of groups */
#define MBEDTLS_ECP_WINDOW_SIZE            2 /**< Maximum window size used */
#define MBEDTLS_ECP_FIXED_POINT_OPTIM      0 /**< Enable fixed-point speed-up */

//#define MBEDTLS_ENTROPY_MAX_SOURCES                20 /**< Maximum number of sources supported */
//#define MBEDTLS_ENTROPY_MAX_GATHER                128 /**< Maximum amount requested from entropy sources */
//#define MBEDTLS_ENTROPY_MIN_HARDWARE               32 /**< Default minimum number of bytes required for the hardware entropy source mbedtls_hardware_poll() before entropy is released */
//#define MBEDTLS_MEMORY_ALIGN_MULTIPLE      4 /**< Align on multiples of this value */

//#define MBEDTLS_PLATFORM_STD_MEM_HDR   <stdlib.h> /**< Header to include if MBEDTLS_PLATFORM_NO_STD_FUNCTIONS is defined. Don't define if no header is needed. */
#define MBEDTLS_PLATFORM_STD_CALLOC        pvPortCalloc /**< Default allocator to use, can be undefined */
#define MBEDTLS_PLATFORM_STD_FREE            vPortFree /**< Default free to use, can be undefined */
//#define MBEDTLS_PLATFORM_STD_EXIT            exit /**< Default exit to use, can be undefined */
//#define MBEDTLS_PLATFORM_STD_TIME            time /**< Default time to use, can be undefined. MBEDTLS_HAVE_TIME must be enabled */
//#define MBEDTLS_PLATFORM_STD_FPRINTF      fprintf /**< Default fprintf to use, can be undefined */
//#define MBEDTLS_PLATFORM_STD_PRINTF        printf /**< Default printf to use, can be undefined */
//#define MBEDTLS_PLATFORM_STD_SNPRINTF    snprintf /**< Default snprintf to use, can be undefined */
//#define MBEDTLS_PLATFORM_STD_EXIT_SUCCESS       0 /**< Default exit value to use, can be undefined */
//#define MBEDTLS_PLATFORM_STD_EXIT_FAILURE       1 /**< Default exit value to use, can be undefined */
//#define MBEDTLS_PLATFORM_STD_NV_SEED_READ   mbedtls_platform_std_nv_seed_read /**< Default nv_seed_read function to use, can be undefined */
//#define MBEDTLS_PLATFORM_STD_NV_SEED_WRITE  mbedtls_platform_std_nv_seed_write /**< Default nv_seed_write function to use, can be undefined */
//#define MBEDTLS_PLATFORM_STD_NV_SEED_FILE  "seedfile" /**< Seed file to read/write with default implementation */
//#define MBEDTLS_PLATFORM_CALLOC_MACRO        calloc /**< Default allocator macro to use, can be undefined */
//#define MBEDTLS_PLATFORM_FREE_MACRO            free /**< Default free macro to use, can be undefined */
//#define MBEDTLS_PLATFORM_EXIT_MACRO            exit /**< Default exit macro to use, can be undefined */
//#define MBEDTLS_PLATFORM_TIME_MACRO            time /**< Default time macro to use, can be undefined. MBEDTLS_HAVE_TIME must be enabled */
//#define MBEDTLS_PLATFORM_TIME_TYPE_MACRO       time_t /**< Default time macro to use, can be undefined. MBEDTLS_HAVE_TIME must be enabled */
//#define MBEDTLS_PLATFORM_FPRINTF_MACRO      fprintf /**< Default fprintf macro to use, can be undefined */
#define MBEDTLS_PLATFORM_PRINTF_MACRO        ets_printf /**< Default printf macro to use, can be undefined */
#define MBEDTLS_PLATFORM_SNPRINTF_MACRO    ets_snprintf /**< Default snprintf macro to use, can be undefined */
#define MBEDTLS_PLATFORM_VSNPRINTF_MACRO    ets_vsnprintf /**< Default vsnprintf macro to use, can be undefined */
//#define MBEDTLS_PLATFORM_NV_SEED_READ_MACRO   mbedtls_platform_std_nv_seed_read /**< Default nv_seed_read function to use, can be undefined */
//#define MBEDTLS_PLATFORM_NV_SEED_WRITE_MACRO  mbedtls_platform_std_nv_seed_write /**< Default nv_seed_write function to use, can be undefined */
//#define MBEDTLS_SSL_CACHE_DEFAULT_TIMEOUT       86400 /**< 1 day  */
//#define MBEDTLS_SSL_CACHE_DEFAULT_MAX_ENTRIES      50 /**< Maximum entries in cache */

#if 0
// dynamic buffer sizing with espconn_secure_set_size()
extern unsigned int max_content_len;
#define MBEDTLS_SSL_MAX_CONTENT_LEN             max_content_len;
#else
// the current mbedtls integration doesn't allow to set the buffer size dynamically:
//   MBEDTLS_SSL_MAX_FRAGMENT_LENGTH feature and dynamic sizing are mutually exclusive
//   due to non-constant initializer element in app/mbedtls/library/ssl_tls.c:150 
// the buffer size is hardcoded here and value is taken from SSL_BUFFER_SIZE (user_config.h)
#define MBEDTLS_SSL_MAX_CONTENT_LEN             SSL_BUFFER_SIZE /**< Maxium fragment length in bytes, determines the size of each of the two internal I/O buffers */
#endif

//#define MBEDTLS_SSL_DEFAULT_TICKET_LIFETIME     86400 /**< Lifetime of session tickets (if enabled) */
//#define MBEDTLS_PSK_MAX_LEN               32 /**< Max size of TLS pre-shared keys, in bytes (default 256 bits) */
//#define MBEDTLS_SSL_COOKIE_TIMEOUT        60 /**< Default expiration delay of DTLS cookies, in seconds if HAVE_TIME, or in number of cookies issued */

//#define MBEDTLS_SSL_CIPHERSUITES MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
#define MBEDTLS_X509_MAX_INTERMEDIATE_CA   3   /**< Maximum number of intermediate CAs in a verification chain. */
//#define MBEDTLS_X509_MAX_FILE_PATH_LEN     512 /**< Maximum length of a path/filename string in bytes including the null terminator character ('\0'). */
