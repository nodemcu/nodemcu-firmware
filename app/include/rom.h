// Headers to the various functions in the rom (as we discover them)

#ifndef _ROM_H_
#define _ROM_H_

#include "c_types.h"

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

// Interrupt Service Routine functions
typedef void (*ets_isr_fn) (void *arg, uint32_t sp);
extern int ets_isr_attach (unsigned int interrupt, ets_isr_fn, void *arg);
extern void ets_isr_mask (unsigned intr);
extern void ets_isr_unmask (unsigned intr);

// Cycle-counter
extern unsigned int xthal_get_ccount (void);
extern int xthal_set_ccompare (unsigned int timer_number, unsigned int compare_value);

// 2, 3 = reset (module dependent?), 4 = wdt
int rtc_get_reset_reason (void);

// Hardware exception handling
struct exception_frame
{
  uint32_t epc;
  uint32_t ps;
  uint32_t sar;
  uint32_t unused;
  union {
    struct {
      uint32_t a0;
      // note: no a1 here!
      uint32_t a2;
      uint32_t a3;
      uint32_t a4;
      uint32_t a5;
      uint32_t a6;
      uint32_t a7;
      uint32_t a8;
      uint32_t a9;
      uint32_t a10;
      uint32_t a11;
      uint32_t a12;
      uint32_t a13;
      uint32_t a14;
      uint32_t a15;
    };
    uint32_t a_reg[15];
  };
  uint32_t cause;
};

/**
 * C-level exception handler callback prototype.
 *
 * Does not need an RFE instruction - it is called through a wrapper which
 * performs state capture & restore, as well as the actual RFE.
 *
 * @param ef An exception frame containing the relevant state from the
 *           exception triggering. This state may be manipulated and will
 *           be applied on return.
 * @param cause The exception cause number.
 */
typedef void (*exception_handler_fn) (struct exception_frame *ef, uint32_t cause);

/**
 * Sets the exception handler function for a particular exception cause.
 * @param handler The exception handler to install.
 *                If NULL, reverts to the XTOS default handler.
 * @returns The previous exception handler, or NULL if none existed prior.
 */
exception_handler_fn _xtos_set_exception_handler (uint32_t cause, exception_handler_fn handler);

#endif
