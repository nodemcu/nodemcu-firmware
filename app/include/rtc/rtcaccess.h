#ifndef RTC_ACCESS_H
#define RTC_ACCESS_H

#include <c_types.h>

#define RTC_MMIO_BASE 0x60000700
#define RTC_USER_MEM_BASE 0x60001200
#define RTC_USER_MEM_NUM_DWORDS 128
#define RTC_TARGET_ADDR 0x04
#define RTC_COUNTER_ADDR 0x1c

static inline uint32_t rtc_mem_read(uint32_t addr)
{
  return ((uint32_t*)RTC_USER_MEM_BASE)[addr];
}

static inline void rtc_mem_write(uint32_t addr, uint32_t val)
{
  ((uint32_t*)RTC_USER_MEM_BASE)[addr]=val;
}

static inline uint64_t rtc_make64(uint32_t high, uint32_t low)
{
  return (((uint64_t)high)<<32)|low;
}

static inline uint64_t rtc_mem_read64(uint32_t addr)
{
  return rtc_make64(rtc_mem_read(addr+1),rtc_mem_read(addr));
}

static inline void rtc_mem_write64(uint32_t addr, uint64_t val)
{
  rtc_mem_write(addr+1,val>>32);
  rtc_mem_write(addr,val&0xffffffff);
}

static inline void rtc_memw(void)
{
  asm volatile ("memw");
}

static inline void rtc_reg_write(uint32_t addr, uint32_t val)
{
  rtc_memw();
  addr+=RTC_MMIO_BASE;
  *((volatile uint32_t*)addr)=val;
  rtc_memw();
}

static inline uint32_t rtc_reg_read(uint32_t addr)
{
  addr+=RTC_MMIO_BASE;
  rtc_memw();
  return *((volatile uint32_t*)addr);
}

static inline void rtc_reg_write_and_loop(uint32_t addr, uint32_t val)
{
  addr+=RTC_MMIO_BASE;
  rtc_memw();
  asm("j 1f\n"
      ".align 32\n"
      "1:\n"
      "s32i.n %1,%0,0\n"
      "2:\n"
      "j 2b\n"::"r"(addr),"r"(val):);
}

#endif
