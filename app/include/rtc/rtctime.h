/*
 * Copyright 2015 Dius Computing Pty Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 * - Neither the name of the copyright holders nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @author Bernd Meyer <bmeyer@dius.com.au>
 * @author Johny Mattsson <jmattsson@dius.com.au>
 */

#ifndef RTCTIME_H
#define RTCTIME_H

#include "rtcaccess.h"

// Layout of the RTC storage space:
//
// 0: Magic. If set to RTC_TIME_MAGIC, the rest is valid. If not, continue to proper boot
//
// 1: cycle counter offset, lower 32 bit
// 2: cycle counter offset, upper 32 bit
//
// 3: cached result of sleep clock calibration. Has the format of system_rtc_clock_cali_proc(),
//    or 0 if not available (see 4/5 below)
// 4: Number of microseconds we tried to sleep, or 0 if we didn't sleep since last calibration, ffffffff if invalid
// 5: Number of RTC cycles we decided to sleep, or 0 if we didn't sleep since last calibration, ffffffff if invalid

// 6: Number of microseconds which we add to (1/2) to avoid time going backwards
// 7: microsecond value returned in the last gettimeofday() to "user space".
//
// (1:2) set to 0 if no time information is available.

//     Entries 4-7 are needed because the RTC cycles/second appears quite temperature dependent,
//     and thus is heavily influenced by what else the chip is doing. As such, any calibration against
//     the crystal-provided clock (which necessarily would have to happen while the chip is active and
//     burning a few milliwatts) will be significantly different from the actual frequency during deep
//     sleep.
//     Thus, in order to calibrate for deep sleep conditions, we keep track of total sleep microseconds
//     and total sleep clock cycles between settimeofday() calls (which presumably are NTP driven), and
//     adjust the calibration accordingly on each settimeofday(). This will also track frequency changes
//     due to ambient temperature changes.
//     6/7 get used when a settimeofday() would result in turning back time. As that can cause all sorts
//     of ugly issues, we *do* adjust (1/2), but compensate by making the same adjustment to (6). Then each
//     time gettimeofday() is called, we inspect (7) and determine how much time has passed since the last
//     call (yes, this gets it wrong if more than a second has passed, but not in a way that causes issues)
//     and try to take up to 6% of that time away from (6) until (6) reaches 0. Also, whenever we go to
//     deep sleep, we add (6) to the sleep time, thus catching up all in one go.
//     Note that for calculating the next sample-aligned wakeup, we need to use the post-adjustment
//     timeofday(), but for calculating actual sleep time, we use the pre-adjustment one, thus bringing
//     things back into line.
//

#define RTC_TIME_BASE  0  // Where the RTC timekeeping block starts in RTC user memory slots
#define RTC_TIME_MAGIC 0x44695573
#define RTC_TIME_MAGIC_SLEEP 0x64697573

// What rate we run the CPU at most of the time, and thus the rate at which we keep our time data
#define CPU_DEFAULT_MHZ 80
#define CPU_BOOTUP_MHZ  52
#define RTC_TIME_CCOMPARE_INT 6 // Interrupt cause for CCOMPARE0 match

// RTCTIME storage
#define RTC_TIME_MAGIC_POS       (RTC_TIME_BASE+0)
#define RTC_CYCLEOFFSETL_POS     (RTC_TIME_BASE+1)
#define RTC_CYCLEOFFSETH_POS     (RTC_TIME_BASE+2)

#define RTC_SLEEPTOTALUS_POS     (RTC_TIME_BASE+3)
#define RTC_SLEEPTOTALCYCLES_POS (RTC_TIME_BASE+4)
#define RTC_TODOFFSETUS_POS      (RTC_TIME_BASE+5)
#define RTC_LASTTODUS_POS        (RTC_TIME_BASE+6)

#define RTC_CALIBRATION_POS      (RTC_TIME_BASE+7)

static inline uint64_t rtc_time_get_now_us_adjusted(uint32_t mhz);

struct rtc_timeval
{
  uint32_t tv_sec;
  uint32_t tv_usec;
};


static inline bool EARLY_ENTRY_ATTR rtc_time_check_sleep_magic(void)
{
  if (rtc_mem_read(RTC_TIME_MAGIC_POS)==RTC_TIME_MAGIC_SLEEP)
    return 1;
  return 0;
}

static inline bool EARLY_ENTRY_ATTR rtc_time_check_wake_magic(void)
{
  if (rtc_mem_read(RTC_TIME_MAGIC_POS)==RTC_TIME_MAGIC)
    return 1;
  return 0;
}

static inline bool EARLY_ENTRY_ATTR rtc_time_check_magic(void)
{
  return rtc_time_check_wake_magic() || rtc_time_check_sleep_magic();
}

static inline void EARLY_ENTRY_ATTR rtc_time_set_wake_magic(void)
{
  rtc_mem_write(RTC_TIME_MAGIC_POS,RTC_TIME_MAGIC);
}

static inline void rtc_time_set_sleep_magic(void)
{
  rtc_mem_write(RTC_TIME_MAGIC_POS,RTC_TIME_MAGIC_SLEEP);
}

static inline void rtc_time_unset_magic(void)
{
  rtc_mem_write(RTC_TIME_MAGIC_POS,0);
}

static inline uint32_t rtc_time_read_raw(void)
{
  return rtc_reg_read(RTC_COUNTER_ADDR);
}

static inline uint32_t rtc_time_read_raw_ccount(void)
{
  return xthal_get_ccount();
}

static inline uint64_t rtc_time_unix_ccount(uint32_t mhz)
{
  // Do *not* cache this, as it can change before the read(s) inside the loop
  if (!rtc_mem_read64(RTC_CYCLEOFFSETL_POS))
    return 0;

  uint64_t result=0;
  // Need to be careful here of race conditions
  while (!result)
  {
    uint32_t before=rtc_time_read_raw_ccount();
    uint64_t base=rtc_mem_read64(RTC_CYCLEOFFSETL_POS);
    uint32_t after=rtc_time_read_raw_ccount();

    if (before<after)
    {
      uint64_t ccount80;
      if (mhz==CPU_DEFAULT_MHZ)
        ccount80=((uint64_t)before+after)/2;
      else
        ccount80=((uint64_t)before+after)*CPU_DEFAULT_MHZ/(2*mhz);
      result=base+ccount80;
    }
  }
  return result;
}

static inline uint64_t rtc_time_unix_us(uint32_t mhz)
{
  return rtc_time_unix_ccount(mhz)/CPU_DEFAULT_MHZ;
}

static inline void rtc_time_register_time_reached(uint32_t s, uint32_t us)
{
  rtc_mem_write(RTC_LASTTODUS_POS,us);
}

static inline uint32_t rtc_time_us_since_time_reached(uint32_t s, uint32_t us)
{
  uint32_t lastus=rtc_mem_read(RTC_LASTTODUS_POS);
  if (us<lastus)
    us+=1000000;
  return us-lastus;
}

// A small sanity check so sleep times go completely nuts if someone
// has provided wrong timestamps to gettimeofday.
static inline bool rtc_time_calibration_is_sane(uint32_t cali)
{
  return (cali>=(4<<12)) && (cali<=(10<<12));
}

static inline void rtc_time_settimeofday(const struct rtc_timeval* tv)
{
  if (!rtc_time_check_magic())
    return;


  uint32_t sleep_us=rtc_mem_read(RTC_SLEEPTOTALUS_POS);
  uint32_t sleep_cycles=rtc_mem_read(RTC_SLEEPTOTALCYCLES_POS);
  // At this point, the CPU clock will definitely be at the default rate (nodemcu fully booted)
  uint64_t now_esp_us=rtc_time_get_now_us_adjusted(CPU_DEFAULT_MHZ);
  uint64_t now_ntp_us=((uint64_t)tv->tv_sec)*1000000+tv->tv_usec;
  int64_t  diff_us=now_esp_us-now_ntp_us;

  // Store the *actual* time.
  uint64_t target_ccount=now_ntp_us*CPU_DEFAULT_MHZ;
  // again, be mindful of race conditions
  while (1)
  {
    uint32_t before=rtc_time_read_raw_ccount();
    rtc_mem_write64(RTC_CYCLEOFFSETL_POS,target_ccount-before);
    uint32_t after=rtc_time_read_raw_ccount();
    if (before<after)
      break;
  }

  // calibrate sleep period based on difference between expected time and actual time
  if (sleep_us>0 && sleep_us<0xffffffff &&
      sleep_cycles>0 && sleep_cycles<0xffffffff)
  {
    uint64_t actual_sleep_us=sleep_us-diff_us;
    uint32_t cali=(actual_sleep_us<<12)/sleep_cycles;
    if (rtc_time_calibration_is_sane(cali))
      rtc_mem_write(RTC_CALIBRATION_POS,cali);
  }

  rtc_mem_write(RTC_SLEEPTOTALUS_POS,0);
  rtc_mem_write(RTC_SLEEPTOTALCYCLES_POS,0);

  // Deal with time adjustment if necessary
  if (diff_us>0) // Time went backwards. Avoid that....
  {
    if (diff_us>0xffffffffULL)
      diff_us=0xffffffffULL;
    now_ntp_us+=diff_us;
  }
  else
    diff_us=0;
  rtc_mem_write(RTC_TODOFFSETUS_POS,diff_us);

  uint32_t now_s=now_ntp_us/1000000;
  uint32_t now_us=now_ntp_us%1000000;
  rtc_time_register_time_reached(now_s,now_us);
}

static inline uint32_t rtc_time_get_calibration(void)
{
  uint32_t cal=rtc_time_check_magic()?rtc_mem_read(RTC_CALIBRATION_POS):0;
  if (!cal)
  {
    // Make a first guess, most likely to be rather bad, but better then nothing.
#ifndef BOOTLOADER_CODE // This will pull in way too much of the system for the bootloader to handle.
    ets_delay_us(200);
    cal=system_rtc_clock_cali_proc();
    rtc_mem_write(RTC_CALIBRATION_POS,cal);
#else
    cal=6<<12;
#endif
  }
  return cal;
}

static inline void rtc_time_invalidate_calibration(void)
{
  rtc_mem_write(RTC_CALIBRATION_POS,0);
}

static inline uint64_t rtc_time_us_to_ticks(uint64_t us)
{
  uint32_t cal=rtc_time_get_calibration();

  return (us<<12)/cal;
}

static inline uint64_t rtc_time_get_now_us_raw(uint32_t mhz)
{
  if (!rtc_time_check_magic())
    return 0;

  return rtc_time_unix_us(mhz);
}

static inline uint64_t rtc_time_get_now_us_adjusted(uint32_t mhz)
{
  uint64_t raw=rtc_time_get_now_us_raw(mhz);
  if (!raw)
    return 0;
  return raw+rtc_mem_read(RTC_TODOFFSETUS_POS);
}

static inline void rtc_time_gettimeofday(struct rtc_timeval* tv, uint32_t mhz)
{
  uint64_t now=rtc_time_get_now_us_adjusted(mhz);
  uint32_t sec=now/1000000;
  uint32_t usec=now%1000000;
  uint32_t to_adjust=rtc_mem_read(RTC_TODOFFSETUS_POS);
  if (to_adjust)
  {
    uint32_t us_passed=rtc_time_us_since_time_reached(sec,usec);
    uint32_t adjust=us_passed>>4;
    if (adjust)
    {
      if (adjust>to_adjust)
        adjust=to_adjust;
      to_adjust-=adjust;
      now-=adjust;
      now/1000000;
      now%1000000;
      rtc_mem_write(RTC_TODOFFSETUS_POS,to_adjust);
    }
  }
  tv->tv_sec=sec;
  tv->tv_usec=usec;
  rtc_time_register_time_reached(sec,usec);
}

static inline void rtc_time_add_sleep_tracking(uint32_t us, uint32_t cycles)
{
  if (rtc_time_check_magic())
  {
    // us is the one that will grow faster...
    uint32_t us_before=rtc_mem_read(RTC_SLEEPTOTALUS_POS);
    uint32_t us_after=us_before+us;
    uint32_t cycles_after=rtc_mem_read(RTC_SLEEPTOTALCYCLES_POS)+cycles;

    if (us_after<us_before) // Give up if it would cause an overflow
    {
      us_after=cycles_after=0xffffffff;
    }
    rtc_mem_write(RTC_SLEEPTOTALUS_POS,    us_after);
    rtc_mem_write(RTC_SLEEPTOTALCYCLES_POS,cycles_after);
  }
}

static void rtc_time_enter_deep_sleep_us(uint32_t us)
{
  if (rtc_time_check_wake_magic())
    rtc_time_set_sleep_magic();

  rtc_reg_write(0,0);
  rtc_reg_write(0,rtc_reg_read(0)&0xffffbfff);
  rtc_reg_write(0,rtc_reg_read(0)|0x30);

  rtc_reg_write(0x44,4);
  rtc_reg_write(0x0c,0x00010010);

  rtc_reg_write(0x48,(rtc_reg_read(0x48)&0xffff01ff)|0x0000fc00);
  rtc_reg_write(0x48,(rtc_reg_read(0x48)&0xfffffe00)|0x00000080);

  rtc_reg_write(RTC_TARGET_ADDR,rtc_time_read_raw()+136);
  rtc_reg_write(0x18,8);
  rtc_reg_write(0x08,0x00100010);

  ets_delay_us(20);

  rtc_reg_write(0x9c,17);
  rtc_reg_write(0xa0,3);

  rtc_reg_write(0x0c,0x640c8);
  rtc_reg_write(0,rtc_reg_read(0)&0xffffffcf);

  uint32_t cycles=rtc_time_us_to_ticks(us);
  rtc_time_add_sleep_tracking(us,cycles);

  rtc_reg_write(RTC_TARGET_ADDR,rtc_time_read_raw()+cycles);
  rtc_reg_write(0x9c,17);
  rtc_reg_write(0xa0,3);

  // Clear bit 0 of DPORT 0x04. Doesn't seem to be necessary
  // wm(0x3fff0004,bitrm(0x3fff0004),0xfffffffe));
  rtc_reg_write(0x40,-1);
  rtc_reg_write(0x44,32);
  rtc_reg_write(0x10,0);

  rtc_reg_write(0x18,8);
  rtc_reg_write(0x08,0x00100000); //  go to sleep
}

static inline void rtc_time_deep_sleep_us(uint32_t us, uint32_t mhz)
{
  if (rtc_time_check_magic())
  {
    uint32_t to_adjust=rtc_mem_read(RTC_TODOFFSETUS_POS);
    if (to_adjust)
    {
      us+=to_adjust;
      rtc_mem_write(RTC_TODOFFSETUS_POS,0);
    }
    uint64_t now=rtc_time_get_now_us_raw(mhz); // Now the same as _adjusted()
    if (now)
    { // Need to maintain the clock first. When we wake up, counter will be 0
      uint64_t wakeup=now+us;
      uint64_t wakeup_cycles=wakeup*CPU_DEFAULT_MHZ;
      rtc_mem_write64(RTC_CYCLEOFFSETL_POS,wakeup_cycles);
    }
  }
  rtc_time_enter_deep_sleep_us(us);
}

static inline void rtc_time_deep_sleep_until_aligned(uint32_t align, uint32_t min_sleep_us, uint32_t mhz)
{
  uint64_t now=rtc_time_get_now_us_adjusted(mhz);
  uint64_t then=now+min_sleep_us;

  if (align)
  {
    then+=align-1;
    then-=(then%align);
  }
  rtc_time_deep_sleep_us(then-now,mhz);
}

static inline void EARLY_ENTRY_ATTR rtc_time_reset(bool clear_cali)
{
  rtc_mem_write64(RTC_CYCLEOFFSETL_POS,0);
  rtc_mem_write(RTC_SLEEPTOTALUS_POS,0);
  rtc_mem_write(RTC_SLEEPTOTALCYCLES_POS,0);
  rtc_mem_write(RTC_TODOFFSETUS_POS,0);
  rtc_mem_write(RTC_LASTTODUS_POS,0);
  if (clear_cali)
    rtc_mem_write(RTC_CALIBRATION_POS,0);
}

static inline void EARLY_ENTRY_ATTR rtc_time_register_bootup(void)
{
  uint32_t reset_reason=rtc_get_reset_reason();
#ifndef BOOTLOADER_CODE
  static const bool erase_calibration=true;
#else
  // In the boot loader, any leftover calibration is going to be better than anything we can
  // come up with....
  static const bool erase_calibration=false;
#endif

  if (rtc_time_check_sleep_magic())
  {
    if (reset_reason!=2)  // This was *not* a proper wakeup from a deep sleep. All our time keeping is f*cked!
      rtc_time_reset(erase_calibration); // Possibly keep the calibration, it should still be good
    rtc_time_set_wake_magic();
    return;
  }

  if (rtc_time_check_wake_magic())
  {
    // This was *not* a proper wakeup from rtc-time initiated deep sleep. All our time keeping is f*cked!
    rtc_time_reset(erase_calibration); // Possibly keep the calibration, it should still be good
  }
}

static inline void EARLY_ENTRY_ATTR rtc_time_switch_to_default_clock(uint32_t mhz)
{
  if (rtc_time_check_magic())
  {
    uint64_t cycles=rtc_time_read_raw_ccount();
    uint64_t missing_cycles=cycles*(CPU_DEFAULT_MHZ-mhz)/mhz;
    uint64_t offset=rtc_mem_read64(RTC_CYCLEOFFSETL_POS);
    if (offset)
    {
      rtc_mem_write64(RTC_CYCLEOFFSETL_POS,offset+missing_cycles);
    }
  }
}

static inline void rtc_time_ccount_wrap_handler(void* dst_v, uint32_t sp)
{
  uint32_t off_h=rtc_mem_read(RTC_CYCLEOFFSETH_POS);
  if (rtc_time_check_magic() && off_h)
  {
    rtc_mem_write(RTC_CYCLEOFFSETH_POS,off_h+1);
  }
  xthal_set_ccompare(0,0); // This resets the interrupt condition
}

static inline void EARLY_ENTRY_ATTR rtc_time_install_wrap_handler(void)
{
  xthal_set_ccompare(0,0); // Recognise a ccounter wraparound
  ets_isr_attach(RTC_TIME_CCOMPARE_INT,rtc_time_ccount_wrap_handler,NULL);
  ets_isr_unmask(1<<RTC_TIME_CCOMPARE_INT);
}


// Call this from the nodemcu entry point, i.e. just before we switch from 52MHz to 80MHz
static inline void EARLY_ENTRY_ATTR rtc_time_switch_clocks(void)
{
  rtc_time_install_wrap_handler();
  rtc_time_switch_to_default_clock(CPU_BOOTUP_MHZ);
}


static inline bool rtc_time_have_time(void)
{
  return (rtc_time_check_magic() && rtc_mem_read64(RTC_CYCLEOFFSETL_POS)!=0);
}

static inline void rtc_time_prepare(void)
{
  rtc_time_reset(true);
  rtc_time_set_wake_magic();
}

#endif
