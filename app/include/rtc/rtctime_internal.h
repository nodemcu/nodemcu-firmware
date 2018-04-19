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

/*
 * It is vital that this file is only included once in the entire
 * system. 
 */

#ifndef _RTCTIME_INTERNAL_H_
#define _RTCTIME_INTERNAL_H_

/*
 * The ESP8266 has four distinct power states:
 *
 * 1) Active ---      CPU and modem are powered and running
 * 2) Modem Sleep --- CPU is active, but the RF section is powered down
 * 3) Light Sleep --- CPU is halted, RF section is powered down. CPU gets reactivated by interrupt
 * 4) Deep Sleep ---  CPU and RF section are powered down, restart requires a full reset
 *
 * There are also three (relevant) sources of time information
 *
 * A) CPU Cycle Counter --- this is incremented at the CPU frequency in modes (1) and (2), but is
 *                    halted in state (3), and gets reset in state (4). Highly precise 32 bit counter
 *                    which overflows roughly every minute. Starts counting as soon as the CPU becomes
 *                    active after a reset. Can cause an interrupt when it hits a particular value;
 *                    This interrupt (and the register that determines the comparison value) are not
 *                    used by the system software, and are available for user code to use.
 *
 * B) Free Running Counter 2 --- This is a peripheral which gets configured to run at 1/256th of the
 *                    CPU frequency. It is also active in states (1) and (2), and is halted in state (3).
 *                    However, the ESP system code will adjust its value across periods of Light Sleep
 *                    that it initiates, so *in effect*, this counter kind-of remains active in (3).
 *                    While in states (1) and (2), it is as precise as the CPU Cycle. While in state (3),
 *                    however, it is only as precise as the system's knowledge of how long the sleep
 *                    period was. This knowledge is limited (it is based on (C), see below).
 *                    The Free Running Counter 2 is a 32 bit counter which overflows roughly every
 *                    4 hours, and typically has a resolution of 3.2us. It starts counting as soon as
 *                    it gets configured, which is considerably *after* the time of reset, and in fact
 *                    is not done by the ESP boot loader, but rather by the loaded-from-SPI-flash system
 *                    code. This means it is not yet running when the boot loader calls the configured
 *                    entry point, and the time between reset and the counter starting to run depends on
 *                    the size of code/data to be copied into RAM from the flash.
 *                    The FRC2 is also used by the system software for its internal time keeping, i.e. for
 *                    dealing with any registered ETS_Timers (and derived-from-them timer functionality).
 *
 * C) "Real Time Clock" --- This peripheral runs from an internal low power RC oscillator, at a frequency
 *                    somewhere in the 120-200kHz range. It keeps running in all power states, and is in
 *                    fact the time source responsible for generating an interrupt (state (3)) or reset
 *                    (state (4)) to end Light and Deep Sleep periods. However, it *does* get reset to
 *                    zero after a reset, even one it caused itself.
 *                    The major issue with the RTC is that it is not using a crystal (support for an
 *                    external 32.768kHz crystal was planned at one point, but was removed from the
 *                    final ESP8266 design), and thus the frequency of the oscillator is dependent on
 *                    a number of parameters, including the chip temperature. The ESP's system software
 *                    contains code to "calibrate" exactly how long one cycle of the oscillator is, and
 *                    uses that calibration to work out how many cycles to sleep for modes (3) and (4).
 *                    However, once the chip has entered a low power state, it quickly cools down, which
 *                    results in the oscillator running faster than during calibration, leading to early
 *                    wakeups. This effect is small (even in relative terms) for short sleep periods (because
 *                    the temperature does not change much over a few hundred milliseconds), but can get
 *                    quite large for extended sleeps.
 *
 * For added fun, a typical ESP8266 module starts up running the CPU (and thus the cycle counter) at 52MHz,
 * but usually this will be switched to 80MHz on application startup, and can later be switched to 160MHz
 * under user control. Meanwhile, the FRC2 is usually kept running at 80MHz/256, regardless of the CPU
 * clock.
 *
 *
 *
 * The code in this file implements a best-effort time keeping solution for the ESP. It keeps track of time
 * by switching between various time sources. All state is kept in RAM associated with the RTC, which is
 * maintained across Deep Sleep periods.
 *
 * Internally, time is managed in units of cycles of a (hypothetical) 2080MHz clock, e.g. in units
 * of 0.4807692307ns. The reason for this choice is that this covers both the FRC2 and the cycle
 * counter periods, while running at 52MHz, 80MHz or 160MHz.
 *
 * At any given time, the time status indicates whether the FRC2 or the Cycle Counter is the current time
 * source, how many unit cycles each LSB of the chosen time source "is worth", and what the unix time in
 * unit cycles was when the time source was at 0.
 * Given that either time source overflows its 32 bit counter in a relatively short time, the code also
 * maintains a "last read 32 bit value" for the selected time source, and on each subsequent read will
 * check for overflow and, if necessary, adjust the unix-time-at-time-source-being-zero appropriately.
 * In order to avoid missing overflows, a timer gets installed which requests time every 40 seconds.
 *
 * To avoid race conditions, *none* of the code here must be called from an interrupt context unless
 * the user can absolutely guarantee that there will never be a clock source rollover (which can be the
 * case for sensor applications that only stay awake for a few seconds). And even then, do so at your
 * own risk.
 *
 *
 * Deep sleep is handled by moving the time offset forward *before* the sleep to the scheduled wakeup
 * time. Due to the nature of the RTC, the actual wakeup time may be a little bit different, but
 * it's the best that can be done. The code attempts to come up with a better calibration value if
 * authoritative time is available both before and after a sleep; This works reasonably well, but of
 * course is still merely a guess, which may well be somewhat wrong.
 *
 */
#include <osapi.h>
#include <ets_sys.h>
#include "rom.h"
#include "rtcaccess.h"
#include "user_interface.h"

// Layout of the RTC storage space:
//
// 0: Magic, and time source. Meaningful values are
//     * RTC_TIME_MAGIC_SLEEP: Indicates that the device went to sleep under RTCTIME control.
//       This is the magic expected on deep sleep wakeup; Any other status means we lost track
//       of time, and whatever time offset is stored in state is invalid and must be cleared.
//     * RTC_TIME_MAGIC_CCOUNT: Time offset is relative to the Cycle Counter.
//     * RTC_TIME_MAGIC_FRC2: Time offset is relative to the Free Running Counter.
//    Any values other than these indicate that RTCTIME is not in use and no state is available, nor should
//    RTCTIME make any changes to any of the RTC memory space.
//
// 1/2: UNIX time in Unit Cycles when time source had value 0 (64 bit, lower 32 bit in 1, upper in 2).
//      If 0, then time is unknown.
// 3: Last used value of time source (32 bit unsigned). If current time source is less, then a rollover happened
// 4: Length of a time source cycle in Unit Cycles.
// 5: cached result of sleep clock calibration. Has the format of system_rtc_clock_cali_proc(),
//    or 0 if not available (see 6/7 below)
// 6: Number of microseconds we tried to sleep, or 0 if we didn't sleep since last calibration, ffffffff if invalid
// 7: Number of RTC cycles we decided to sleep, or 0 if we didn't sleep since last calibration, ffffffff if invalid

// 8: Number of microseconds which we add to (1/2) to avoid time going backwards
// 9: microsecond value returned in the last gettimeofday() to "user space".
//
//     Entries 6-9 are needed because the RTC cycles/second appears quite temperature dependent,
//     and thus is heavily influenced by what else the chip is doing. As such, any calibration against
//     the crystal-provided clock (which necessarily would have to happen while the chip is active and
//     burning a few milliwatts) will be significantly different from the actual frequency during deep
//     sleep.
//     Thus, in order to calibrate for deep sleep conditions, we keep track of total sleep microseconds
//     and total sleep clock cycles between settimeofday() calls (which presumably are NTP driven), and
//     adjust the calibration accordingly on each settimeofday(). This will also track frequency changes
//     due to ambient temperature changes.
//     8/9 get used when a settimeofday() would result in turning back time. As that can cause all sorts
//     of ugly issues, we *do* adjust (1/2), but compensate by making the same adjustment to (8). Then each
//     time gettimeofday() is called, we inspect (9) and determine how much time has passed since the last
//     call (yes, this gets it wrong if more than a second has passed, but not in a way that causes issues)
//     and try to take up to 6% of that time away from (8) until (8) reaches 0. Also, whenever we go to
//     deep sleep, we add (8) to the sleep time, thus catching up all in one go.
//     Note that for calculating the next sample-aligned wakeup, we need to use the post-adjustment
//     timeofday(), but for calculating actual sleep time, we use the pre-adjustment one, thus bringing
//     things back into line.
//

#define RTC_TIME_BASE  0  // Where the RTC timekeeping block starts in RTC user memory slots
#define RTC_TIME_MAGIC_CCOUNT 0x44695573
#define RTC_TIME_MAGIC_FRC2   (RTC_TIME_MAGIC_CCOUNT+1)
#define RTC_TIME_MAGIC_SLEEP  (RTC_TIME_MAGIC_CCOUNT+2)

#define UNITCYCLE_MHZ     2080
#define CPU_OVERCLOCK_MHZ 160
#define CPU_DEFAULT_MHZ   80
#define CPU_BOOTUP_MHZ    52

#ifdef RTC_DEBUG_ENABLED
#warn this does not really work....
#define RTC_DBG(...)         do { if (rtc_dbg_enabled == 'R') { dbg_printf(__VA_ARGS__); } } while (0)
static char rtc_dbg_enabled;
#define RTC_DBG_ENABLED()   rtc_dbg_enabled = 'R'
#define RTC_DBG_NOT_ENABLED()   rtc_dbg_enabled = 0
#else
#define RTC_DBG(...)
#define RTC_DBG_ENABLED()
#define RTC_DBG_NOT_ENABLED()
#endif

#define NOINIT_ATTR __attribute__((section(".noinit")))

// RTCTIME storage
#define RTC_TIME_MAGIC_POS       (RTC_TIME_BASE+0)
#define RTC_CYCLEOFFSETL_POS     (RTC_TIME_BASE+1)
#define RTC_CYCLEOFFSETH_POS     (RTC_TIME_BASE+2)
#define RTC_LASTSOURCEVAL_POS    (RTC_TIME_BASE+3)
#define RTC_SOURCECYCLEUNITS_POS (RTC_TIME_BASE+4)
#define RTC_CALIBRATION_POS      (RTC_TIME_BASE+5)
#define RTC_SLEEPTOTALUS_POS     (RTC_TIME_BASE+6)
#define RTC_SLEEPTOTALCYCLES_POS (RTC_TIME_BASE+7)
//#define RTC_TODOFFSETUS_POS      (RTC_TIME_BASE+8)
//#define RTC_LASTTODUS_POS        (RTC_TIME_BASE+9)
#define RTC_USRATE_POS		 (RTC_TIME_BASE+8)

NOINIT_ATTR uint32_t rtc_time_magic;
NOINIT_ATTR uint64_t rtc_cycleoffset;
NOINIT_ATTR uint32_t rtc_lastsourceval;
NOINIT_ATTR uint32_t rtc_sourcecycleunits;
NOINIT_ATTR uint32_t rtc_calibration;
NOINIT_ATTR uint32_t rtc_sleeptotalus;
NOINIT_ATTR uint32_t rtc_sleeptotalcycles;
NOINIT_ATTR uint64_t rtc_usatlastrate;
NOINIT_ATTR uint64_t rtc_rateadjustedus;
NOINIT_ATTR uint32_t rtc_todoffsetus;
NOINIT_ATTR uint32_t rtc_lasttodus;
NOINIT_ATTR uint32_t rtc_usrate;


struct rtc_timeval
{
  uint32_t tv_sec;
  uint32_t tv_usec;
};

static void bbram_load() {
  rtc_time_magic = rtc_mem_read(RTC_TIME_MAGIC_POS);
  rtc_cycleoffset = rtc_mem_read64(RTC_CYCLEOFFSETL_POS);
  rtc_lastsourceval = rtc_mem_read(RTC_LASTSOURCEVAL_POS);
  rtc_sourcecycleunits = rtc_mem_read(RTC_SOURCECYCLEUNITS_POS);
  rtc_calibration = rtc_mem_read(RTC_CALIBRATION_POS);
  rtc_sleeptotalus = rtc_mem_read(RTC_SLEEPTOTALUS_POS);
  rtc_sleeptotalcycles = rtc_mem_read(RTC_SLEEPTOTALCYCLES_POS);
  rtc_usrate = rtc_mem_read(RTC_USRATE_POS);
}

static void bbram_save() {
  RTC_DBG("bbram_save\n");
  rtc_mem_write(RTC_TIME_MAGIC_POS       , rtc_time_magic);
  rtc_mem_write64(RTC_CYCLEOFFSETL_POS   , rtc_cycleoffset);
  rtc_mem_write(RTC_LASTSOURCEVAL_POS    , rtc_lastsourceval);
  rtc_mem_write(RTC_SOURCECYCLEUNITS_POS , rtc_sourcecycleunits);
  rtc_mem_write(RTC_CALIBRATION_POS      , rtc_calibration);
  rtc_mem_write(RTC_SLEEPTOTALUS_POS     , rtc_sleeptotalus);
  rtc_mem_write(RTC_SLEEPTOTALCYCLES_POS , rtc_sleeptotalcycles);
  rtc_mem_write(RTC_USRATE_POS		 , rtc_usrate);
}

static inline uint64_t div2080(uint64_t n) {
  n = n >> 5;
  uint64_t t = n >> 7;

  uint64_t q = t + (t >> 1) + (t >> 2);

  q += q >> 3;
  q += q >> 12;
  q += q >> 24;
  q += q >> 48;

  uint32_t r = (uint32_t) n - (uint32_t) q * 65;

  uint32_t off = (r - (r >> 6)) >> 6;

  q = q + off;

  return q;
}

static uint32_t div1m(uint32_t *rem, unsigned long long n) {
  // 0   -> 0002000000000000 sub
  // 0 >> 5 - 0 >> 19 + [-1]  -> 00020fffc0000000 add
  // 0 >> 9 - 0 >> 6 + [-1]  -> 000208ffc0000000 add
  // 2 >> 12 - 2 >> 23   -> 000000208bea0080 sub

  uint64_t q1 = (n >> 5) - (n >> 19) + n;
  uint64_t q2 = q1 + (n >> 9) - (n >> 6);
  uint64_t q3 = (q2 >> 12) - (q2 >> 23);

  uint64_t q = q1 - n + q2 - q3;

  q = q >> 20;

  uint32_t r = (uint32_t) n - (uint32_t) q * 1000000;

  if (r >= 1000000) {
    r -= 1000000;
    q++;
  }

  *rem = r;

  return q;
}

static inline uint64_t rtc_time_get_now_us_adjusted();

#define rtc_time_get_magic()    rtc_time_magic

static inline bool rtc_time_check_sleep_magic(void)
{
  uint32_t magic=rtc_time_get_magic();
  return (magic==RTC_TIME_MAGIC_SLEEP);
}

static inline bool rtc_time_check_wake_magic(void)
{
  uint32_t magic=rtc_time_get_magic();
  return (magic==RTC_TIME_MAGIC_FRC2 || magic==RTC_TIME_MAGIC_CCOUNT);
}

static inline bool rtc_time_check_magic(void)
{
  uint32_t magic=rtc_time_get_magic();
  return (magic==RTC_TIME_MAGIC_FRC2 || magic==RTC_TIME_MAGIC_CCOUNT || magic==RTC_TIME_MAGIC_SLEEP);
}

static void rtc_time_set_magic(uint32_t new_magic)
{
  RTC_DBG("Set magic to %08x\n", new_magic);
  rtc_time_magic = new_magic;
  bbram_save();
}

static inline void rtc_time_set_sleep_magic(void)
{
  rtc_time_set_magic(RTC_TIME_MAGIC_SLEEP);
}

static inline void rtc_time_set_ccount_magic(void)
{
  rtc_time_set_magic(RTC_TIME_MAGIC_CCOUNT);
}

static inline void rtc_time_set_frc2_magic(void)
{
  rtc_time_set_magic(RTC_TIME_MAGIC_FRC2);
}



static inline void rtc_time_unset_magic(void)
{
  rtc_time_set_magic(0);
}

static inline uint32_t rtc_time_read_raw(void)
{
  return rtc_reg_read(RTC_COUNTER_ADDR);
}

static inline uint32_t rtc_time_read_raw_ccount(void)
{
  return xthal_get_ccount();
}

static inline uint32_t rtc_time_read_raw_frc2(void)
{
  return NOW();
}

// Get us the number of Unit Cycles that have elapsed since the source was 0.
// Note: This may in fact adjust the stored cycles-when-source-was-0 entry, so
// we need to make sure we call this before reading that entry
static inline uint64_t rtc_time_source_offset(void)
{
  uint32_t magic=rtc_time_get_magic();
  uint32_t raw=0;

  switch (magic)
  {
  case RTC_TIME_MAGIC_CCOUNT: raw=rtc_time_read_raw_ccount(); break;
  case RTC_TIME_MAGIC_FRC2:   raw=rtc_time_read_raw_frc2();   break;
  default: return 0; // We are not in a position to offer time
  }
  uint32_t multiplier = rtc_sourcecycleunits;
  uint32_t previous = rtc_lastsourceval;
  if (raw<previous)
  { // We had a rollover.
    uint64_t to_add=(1ULL<<32)*multiplier;
    uint64_t base = rtc_cycleoffset;
    if (base)
      rtc_cycleoffset = base + to_add;
  }
  rtc_lastsourceval = raw;
  return ((uint64_t)raw)*multiplier;
}

static inline uint64_t rtc_time_unix_unitcycles(void)
{
  // Note: The order of these two must be maintained, as the first call might change the outcome of the second
  uint64_t offset=rtc_time_source_offset();
  uint64_t base = rtc_cycleoffset;

  if (!base)
    return 0; // No known time

  return base+offset;
}

static inline uint64_t rtc_time_unix_us(void)
{
#if UNITCYCLE_MHZ == 2080
  return div2080(rtc_time_unix_unitcycles());
#else
#error UNITCYCLE_MHZ has an odd value
  return rtc_time_unix_unitcycles()/UNITCYCLE_MHZ;
#endif
}

static inline void rtc_time_register_time_reached(uint32_t s, uint32_t us)
{
  rtc_lasttodus = us;
}

static inline uint32_t rtc_time_us_since_time_reached(uint32_t s, uint32_t us)
{
  uint32_t lastus=rtc_lasttodus;
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


static inline uint32_t rtc_time_get_calibration(void)
{
  uint32_t cal=rtc_time_check_magic()?rtc_calibration:0;
  if (!cal)
  {
    // Make a first guess, most likely to be rather bad, but better then nothing.
#ifndef BOOTLOADER_CODE // This will pull in way too much of the system for the bootloader to handle.
    ets_delay_us(200);
    cal=system_rtc_clock_cali_proc();
    rtc_calibration = cal;
#else
    cal=6<<12;
#endif
  }
  return cal;
}

static inline void rtc_time_invalidate_calibration(void)
{
  rtc_calibration = 0;
}

static inline uint64_t rtc_time_us_to_ticks(uint64_t us)
{
  uint32_t cal=rtc_time_get_calibration();

  return (us<<12)/cal;
}

static inline uint64_t rtc_time_get_now_us_raw(void)
{
  if (!rtc_time_check_magic())
    return 0;

  return rtc_time_unix_us();
}

static inline uint64_t rtc_time_get_now_us_adjusted(void)
{
  uint64_t raw=rtc_time_get_now_us_raw();
  if (!raw)
    return 0;
  return raw+rtc_todoffsetus;
}


static inline void rtc_time_add_sleep_tracking(uint32_t us, uint32_t cycles)
{
  if (rtc_time_check_magic())
  {
    // us is the one that will grow faster...
    uint32_t us_before=rtc_sleeptotalus;
    uint32_t us_after=us_before+us;
    uint32_t cycles_after=rtc_sleeptotalcycles+cycles;

    if (us_after<us_before) // Give up if it would cause an overflow
    {
      us_after=cycles_after=0xffffffff;
    }
    rtc_sleeptotalus = us_after;
    rtc_sleeptotalcycles = cycles_after;
  }
}

extern void rtc_time_enter_deep_sleep_final(void);

static void rtc_time_enter_deep_sleep_us(uint32_t us)
{
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

  if (rtc_time_check_wake_magic())
    rtc_time_set_sleep_magic();
  else 
    bbram_save();

  rtc_reg_write(RTC_TARGET_ADDR,rtc_time_read_raw()+cycles);
  rtc_reg_write(0x9c,17);
  rtc_reg_write(0xa0,3);

  volatile uint32_t* dport4=(volatile uint32_t*)0x3ff00004;
  *dport4&=0xfffffffe;

  rtc_reg_write(0x40,-1);
  rtc_reg_write(0x44,32);
  rtc_reg_write(0x10,0);

  rtc_time_enter_deep_sleep_final();
}

static void rtc_time_deep_sleep_us(uint32_t us)
{
  if (rtc_time_check_magic())
  {
    uint32_t to_adjust=rtc_todoffsetus;
    if (to_adjust)
    {
      us+=to_adjust;
      rtc_todoffsetus = 0;
    }
    uint64_t now=rtc_time_get_now_us_raw(); // Now the same as _adjusted()
    if (now)
    { // Need to maintain the clock first. When we wake up, counter will be 0
      uint64_t wakeup=now+us;
      uint64_t wakeup_cycles=wakeup*UNITCYCLE_MHZ;
      // Probly should factor in the rate here TODO
      rtc_cycleoffset = wakeup_cycles;
    }
  }
  rtc_time_enter_deep_sleep_us(us);
}

static inline void rtc_time_deep_sleep_until_aligned(uint32_t align, uint32_t min_sleep_us)
{
  uint64_t now=rtc_time_get_now_us_adjusted();
  uint64_t then=now+min_sleep_us;

  if (align)
  {
    then+=align-1;
    then-=(then%align);
  }
  rtc_time_deep_sleep_us(then-now);
}

static void rtc_time_reset(bool clear_cali)
{
  rtc_cycleoffset = 0;
  rtc_sleeptotalus = 0;
  rtc_sleeptotalcycles = 0;
  rtc_todoffsetus = 0;
  rtc_lasttodus = 0;
  rtc_sourcecycleunits = 0;
  rtc_lastsourceval = 0;
  rtc_usatlastrate = 0;
  rtc_rateadjustedus = 0;
  rtc_usrate = 0;

  if (clear_cali)
    rtc_calibration = 0;

  bbram_save();
}

static inline bool rtc_time_have_time(void)
{
  return (rtc_time_check_magic() && rtc_cycleoffset!=0);
}


static void rtc_time_select_frc2_source()
{
  // FRC2 always runs at 1/256th of the default 80MHz clock, even if the actual clock is different
  uint32_t new_multiplier=(256*UNITCYCLE_MHZ+CPU_DEFAULT_MHZ/2)/CPU_DEFAULT_MHZ;

  uint64_t now;
  uint32_t before;
  uint32_t after;

  // Deal with race condition here...
  do {
    before=rtc_time_read_raw_frc2();
    now=rtc_time_unix_unitcycles();
    after=rtc_time_read_raw_frc2();
  } while (before>after);

  if (rtc_time_have_time())
  {
    uint64_t offset=(uint64_t)after*new_multiplier;
    rtc_cycleoffset = now-offset;
    rtc_lastsourceval = after;
  }
  rtc_sourcecycleunits = new_multiplier;
  rtc_time_set_magic(RTC_TIME_MAGIC_FRC2);
}

static void rtc_time_select_ccount_source(uint32_t mhz, bool first)
{
  uint32_t new_multiplier=(UNITCYCLE_MHZ+mhz/2)/mhz;

  // Check that
  if (new_multiplier*mhz!=UNITCYCLE_MHZ)
    ets_printf("Trying to use unsuitable frequency: %dMHz\n",mhz);

  if (first)
  { // The ccounter has been running at this rate since startup, and the offset is set accordingly
    rtc_lastsourceval = 0;
    rtc_sourcecycleunits = new_multiplier;
    rtc_time_set_magic(RTC_TIME_MAGIC_CCOUNT);
    return;
  }

  uint64_t now;
  uint32_t before;
  uint32_t after;

  // Deal with race condition here...
  do {
    before=rtc_time_read_raw_ccount();
    now=rtc_time_unix_unitcycles();
    after=rtc_time_read_raw_ccount();
  } while (before>after);

  if (rtc_time_have_time())
  {
    uint64_t offset=(uint64_t)after*new_multiplier;
    rtc_cycleoffset = now-offset;
    rtc_lastsourceval = after;
  }
  rtc_sourcecycleunits = new_multiplier;
  rtc_time_set_magic(RTC_TIME_MAGIC_CCOUNT);
}



static inline void rtc_time_switch_to_ccount_frequency(uint32_t mhz)
{
  if (rtc_time_check_magic())
    rtc_time_select_ccount_source(mhz,false);
}

static inline void rtc_time_switch_to_system_clock(void)
{
  if (rtc_time_check_magic())
    rtc_time_select_frc2_source();
}

static inline void rtc_time_tmrfn(void* arg);

#include "pm/swtimer.h"

static void rtc_time_install_timer(void)
{
  static ETSTimer tmr;

  os_timer_setfn(&tmr,rtc_time_tmrfn,NULL);
  SWTIMER_REG_CB(rtc_time_tmrfn, SWTIMER_RESUME);
    //I believe the function rtc_time_tmrfn compensates for drift in the clock and updates rtc time accordingly, This timer should probably be resumed
  os_timer_arm(&tmr,10000,1);
}

#if 0 // Kept around for reference....
static inline void rtc_time_ccount_wrap_handler(void* dst_v, uint32_t sp)
{
  uint32_t off_h=rtc_mem_read(RTC_CYCLEOFFSETH_POS);
  if (rtc_time_check_magic() && off_h)
  {
    rtc_mem_write(RTC_CYCLEOFFSETH_POS,off_h+1);
  }
  xthal_set_ccompare(0,0); // This resets the interrupt condition
}

static inline void rtc_time_install_wrap_handler(void)
{
  xthal_set_ccompare(0,0); // Recognise a ccounter wraparound
  ets_isr_attach(RTC_TIME_CCOMPARE_INT,rtc_time_ccount_wrap_handler,NULL);
  ets_isr_unmask(1<<RTC_TIME_CCOMPARE_INT);
}
#endif












// This switches from MAGIC_SLEEP to MAGIC_CCOUNT, with ccount running at bootup frequency (i.e. 52MHz).
// To be called as early as possible, potententially as the first thing in an overridden entry point.
static void rtc_time_register_bootup(void)
{
  RTC_DBG_NOT_ENABLED();
  bbram_load();
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
    rtc_time_select_ccount_source(CPU_BOOTUP_MHZ,true);
    rtc_todoffsetus = 0;
    rtc_rateadjustedus = rtc_usatlastrate = rtc_time_get_now_us_adjusted();
    RTC_DBG_ENABLED();
    return;
  }

  // We did not go to sleep properly. All our time keeping is f*cked!
  rtc_time_reset(erase_calibration); // Possibly keep the calibration, it should still be good

  RTC_DBG_ENABLED();
}

// Call this from the nodemcu entry point, i.e. just before we switch from 52MHz to 80MHz
static inline void rtc_time_switch_clocks(void)
{
  rtc_time_switch_to_ccount_frequency(CPU_DEFAULT_MHZ);
}

// Call this exactly once, from user_init, i.e. once the operating system is up and running
static inline void rtc_time_switch_system(void)
{
  rtc_time_install_timer();
  rtc_time_switch_to_system_clock();
}


static inline void rtc_time_prepare(void)
{
  rtc_time_reset(true);
  rtc_time_select_frc2_source();
}

static int32_t rtc_time_adjust_delta_by_rate(int32_t delta) {
  return (delta * ((1ull << 32) + (int) rtc_usrate)) >> 32;
}

static uint64_t rtc_time_adjust_us_by_rate(uint64_t us, int force) {
  uint64_t usoff = us - rtc_usatlastrate; 
  uint64_t usadj = (usoff * ((1ull << 32) + (int) rtc_usrate)) >> 32;
  usadj = usadj + rtc_rateadjustedus;

  if (usoff > 1000000000 || force) {
    rtc_usatlastrate = us;
    rtc_rateadjustedus = usadj;
  }

  return usadj;
}

static inline void rtc_time_set_rate(int32_t rate) {
  uint64_t now=rtc_time_get_now_us_adjusted();
  rtc_time_adjust_us_by_rate(now, 1);
  rtc_usrate = rate;
}

static inline int32_t rtc_time_get_rate() {
  return rtc_usrate;
}

static inline void rtc_time_tmrfn(void* arg)
{
  uint64_t now=rtc_time_get_now_us_adjusted();
  rtc_time_adjust_us_by_rate(now, 0);
  rtc_time_source_offset();
}


static inline void rtc_time_gettimeofday(struct rtc_timeval* tv)
{
  uint64_t now=rtc_time_get_now_us_adjusted();
  now = rtc_time_adjust_us_by_rate(now, 0);
  uint32_t usec;
  uint32_t sec = div1m(&usec, now);
  uint32_t to_adjust=rtc_todoffsetus;
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
      sec = div1m(&usec, now);
      rtc_todoffsetus = to_adjust;
    }
  }
  tv->tv_sec=sec;
  tv->tv_usec=usec;
  rtc_time_register_time_reached(sec,usec);
}

static void rtc_time_settimeofday(const struct rtc_timeval* tv)
{
  if (!rtc_time_check_magic())
    return;


  uint32_t sleep_us=rtc_sleeptotalus;
  uint32_t sleep_cycles=rtc_sleeptotalcycles;
  // At this point, the CPU clock will definitely be at the default rate (nodemcu fully booted)
  uint64_t now_esp_us=rtc_time_get_now_us_adjusted();
  now_esp_us = rtc_time_adjust_us_by_rate(now_esp_us, 1);
  uint64_t now_ntp_us=((uint64_t)tv->tv_sec)*1000000+tv->tv_usec;
  int64_t  diff_us=now_esp_us-now_ntp_us;

  // Store the *actual* time.
  uint64_t target_unitcycles=now_ntp_us*UNITCYCLE_MHZ;
  uint64_t sourcecycles=rtc_time_source_offset();
  rtc_cycleoffset = target_unitcycles-sourcecycles;

  // calibrate sleep period based on difference between expected time and actual time
  if (sleep_us>0 && sleep_us<0xffffffff &&
      sleep_cycles>0 && sleep_cycles<0xffffffff &&
      tv->tv_sec)
  {
    uint64_t actual_sleep_us=sleep_us-diff_us;
    uint32_t cali=(actual_sleep_us<<12)/sleep_cycles;
    if (rtc_time_calibration_is_sane(cali))
      rtc_calibration = cali;
  }

  rtc_sleeptotalus = 0;
  rtc_sleeptotalcycles = 0;

  rtc_usatlastrate = now_ntp_us;
  rtc_rateadjustedus = now_ntp_us;
  rtc_usrate = 0;

  // Deal with time adjustment if necessary
  if (diff_us>0 && tv->tv_sec) // Time went backwards. Avoid that....
  {
    if (diff_us>0xffffffffULL)
      diff_us=0xffffffffULL;
    now_ntp_us+=diff_us;
  } else {
    diff_us=0;
  }

  rtc_todoffsetus = diff_us;

  uint32_t now_s=now_ntp_us/1000000;
  uint32_t now_us=now_ntp_us%1000000;
  rtc_time_register_time_reached(now_s,now_us);
}

#endif
