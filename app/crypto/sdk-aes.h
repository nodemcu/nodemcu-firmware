#ifndef _SDK_AES_H_
#define _SDK_AES_H_

#define AES_BLOCKSIZE 16

void *aes_encrypt_init (const char *key, size_t len);
void aes_encrypt (void *ctx, const char *plain, char *crypt);
void aes_encrypt_deinit (void *ctx);

void *aes_decrypt_init (const char *key, size_t len);
void aes_decrypt (void *ctx, const char *crypt, char *plain);
void aes_decrypt_deinit (void *ctx);

#endif
