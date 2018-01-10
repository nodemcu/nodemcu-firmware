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
 * @author Johny Mattsson <jmattsson@dius.com.au>
 */
#ifndef _RTCTIME_H_
#define _RTCTIME_H_

/* We don't want to expose the raw rtctime interface as it is heavily
 * 'static inline' and used by a few things, so instead we wrap the
 * relevant functions and expose these instead, through the rtctime.c module.
 */

#include <c_types.h>
#include "sections.h"

#ifndef _RTCTIME_INTERNAL_H_
struct rtc_timeval
{
  uint32_t tv_sec;
  uint32_t tv_usec;
};
#endif

struct rtc_tm{
  int tm_sec;   /* Seconds.     [0-60] (1 leap second) */
  int tm_min;   /* Minutes.     [0-59] */
  int tm_hour;  /* Hours.       [0-23] */
  int tm_mday;  /* Day.         [1-31] */
  int tm_mon;   /* Month.       [0-11] */
  int tm_year;  /* Year - 1900. */
  int tm_wday;  /* Day of week. [0-6] */
  int tm_yday;  /* Days in year.[0-365]	*/
};

void TEXT_SECTION_ATTR rtctime_early_startup (void);
void rtctime_late_startup (void);
void rtctime_adjust_rate (int rate);
void rtctime_gettimeofday (struct rtc_timeval *tv);
void rtctime_settimeofday (const struct rtc_timeval *tv);
bool rtctime_have_time (void);
void rtctime_deep_sleep_us (uint32_t us);
void rtctime_deep_sleep_until_aligned_us (uint32_t align_us, uint32_t min_us);
void rtctime_gmtime (const int32 stamp, struct rtc_tm *r);

#endif
