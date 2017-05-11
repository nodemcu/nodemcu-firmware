// Headers to the various functions in the rom (as we discover them)

#ifndef _ROM_H_
#define _ROM_H_

#include "c_types.h"
#include "ets_sys.h"

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
typedef void (*ets_isr_fn) (void *arg);
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


void ets_update_cpu_frequency (uint32_t mhz);
uint32_t ets_get_cpu_frequency (void);

void *ets_memcpy (void *dst, const void *src, size_t n);
void *ets_memmove (void *dst, const void *src, size_t n);
void *ets_memset (void *dst, int c, size_t n);
int ets_memcmp (const void *s1, const void *s2, size_t n);

char *ets_strcpy (char *dst, const char *src);
int ets_strcmp (const char *s1, const char *s2);
int ets_strncmp (const char *s1, const char *s2, size_t n);
char *ets_strncpy(char *dest, const char *src, size_t n);
char *ets_strstr(const char *haystack, const char *needle);

int ets_printf(const char *format, ...)  __attribute__ ((format (printf, 1, 2)));

void ets_str2macaddr (uint8_t *dst, const char *str);

void ets_timer_disarm (ETSTimer *a);
void ets_timer_setfn (ETSTimer *t, ETSTimerFunc *fn, void *parg);

void Cache_Read_Enable(uint32_t b0, uint32_t b1, uint32_t use_40108000);
void Cache_Read_Disable(void);

void ets_intr_lock(void);
void ets_intr_unlock(void);

int rand(void);
void srand(unsigned int);

/* Returns 0 on success, 1 on failure */
uint8_t SPIRead(uint32_t src_addr, uint32_t *des_addr, uint32_t size);
uint8_t SPIWrite(uint32_t dst_addr, const uint32_t *src, uint32_t size);
uint8_t SPIEraseSector(uint32_t sector);

#endif
