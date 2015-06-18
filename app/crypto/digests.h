#ifndef _CRYPTO_DIGESTS_H_
#define _CRYPTO_DIGESTS_H_

#include <c_types.h>

typedef void (*create_ctx_fn)(void *ctx);
typedef void (*update_ctx_fn)(void *ctx, const uint8_t *msg, int len);
typedef void (*finalize_ctx_fn)(uint8_t *digest, void *ctx);

/**
 * Description of a message digest mechanism.
 *
 * Typical usage (if not using the crypto_xxxx() functions below):
 *   digest_mech_info_t *mi = crypto_digest_mech (chosen_algorithm);
 *   void *ctx = os_malloc (mi->ctx_size);
 *   mi->create (ctx);
 *   mi->update (ctx, data, len);
 *   ...
 *   uint8_t *digest = os_malloc (mi->digest_size);
 *   mi->finalize (digest, ctx);
 *   ...
 *   os_free (ctx);
 *   os_free (digest);
 */
typedef struct
{
  /* Note: All entries are 32bit to enable placement using ICACHE_RODATA_ATTR.*/
  const char *    name;
  create_ctx_fn   create;
  update_ctx_fn   update;
  finalize_ctx_fn finalize;
  uint32_t        ctx_size;
  uint32_t        digest_size;
  uint32_t        block_size;
} digest_mech_info_t;


/**
 * Looks up the mech data for a specified digest algorithm.
 * @param mech The name of the algorithm, e.g. "MD5", "SHA256"
 * @returns The mech data, or null if the mech is unknown.
 */
const digest_mech_info_t *crypto_digest_mech (const char *mech);

/**
 * Wrapper function for performing a one-in-all hashing operation.
 * @param mi       A mech from @c crypto_digest_mech(). A null pointer @c mi
 *                 is harmless, but will of course result in an error return.
 * @param data     The data to create a digest for.
 * @param data_len Number of bytes at @c data to digest.
 * @param digest   Output buffer, must be at least @c mi->digest_size in size.
 * @return 0 on success, non-zero on error.
 */
int crypto_hash (const digest_mech_info_t *mi, const char *data, size_t data_len, uint8_t *digest);


/**
 * Generate a HMAC signature.
 * @param mi       A mech from @c crypto_digest_mech(). A null pointer @c mi
 *                 is harmless, but will of course result in an error return.
 * @param data     The data to generate a signature for.
 * @param data_len Number of bytes at @c data to process.
 * @param key      The key to use.
 * @param key_len  Number of bytes the @c key comprises.
 * @param digest   Output buffer, must be at least @c mi->digest_size in size.
 * @return 0 on success, non-zero on error.
 */
int crypto_hmac (const digest_mech_info_t *mi, const char *data, size_t data_len, const char *key, size_t key_len, uint8_t *digest);

/**
 * Perform ASCII Hex encoding. Does not null-terminate the buffer.
 *
 * @param bin     The buffer to ascii-hex encode.
 * @param bin_len Number of bytes in @c bin to encode.
 * @param outbuf  Output buffer, must be at least @c bin_len*2 bytes in size.
 *                Note that in-place encoding is supported, and as such
 *                bin==outbuf is safe, provided the buffer is large enough.
 */
void crypto_encode_asciihex (const char *bin, size_t bin_len, char *outbuf);


/** Text string "0123456789abcdef" */
const char crypto_hexbytes[17];

#endif
