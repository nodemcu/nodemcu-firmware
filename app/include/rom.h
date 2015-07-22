// Headers to the various functions in the rom (as we discover them)

// SHA1 is assumed to match the netbsd sha1.h headers
#define SHA1_DIGEST_LENGTH		20
#define SHA1_DIGEST_STRING_LENGTH	41

typedef struct {
	uint32_t state[5];
	uint32_t count[2];
	uint8_t buffer[64];
} SHA1_CTX;

extern void SHA1Transform(uint32_t[5], const uint8_t[64]);
extern void SHA1Init(SHA1_CTX *);
extern void SHA1Final(uint8_t[SHA1_DIGEST_LENGTH], SHA1_CTX *);
extern void SHA1Update(SHA1_CTX *, const uint8_t *, unsigned int);


// MD5 is assumed to match the NetBSD md5.h header
#define MD5_DIGEST_LENGTH		16
typedef struct
{
  uint32_t state[5];
  uint32_t count[2];
  uint8_t buffer[64];
} MD5_CTX;

extern void MD5Init(MD5_CTX *);
extern void MD5Update(MD5_CTX *, const unsigned char *, unsigned int);
extern void MD5Final(unsigned char[MD5_DIGEST_LENGTH], MD5_CTX *);

// base64_encode/decode derived by Cal
// Appears to match base64.h from netbsd wpa utils.
extern unsigned char * base64_encode(const unsigned char *src, size_t len, size_t *out_len);
extern unsigned char * base64_decode(const unsigned char *src, size_t len, size_t *out_len);
// Unfortunately it that seems to require the ROM memory management to be
// initialized because it uses mem_malloc

extern void mem_init(void * start_addr);
