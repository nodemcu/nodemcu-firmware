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
 */
#ifndef _RTCFIFO_H_
#define _RTCFIFO_H_
#include "rtcaccess.h"
#include "rtctime.h"

// 1: measurement alignment, in microseconds
// 2: timestamp for next sample (seconds). For sensors which sense during the sleep phase. Set to
//    0 to indicate no sample waiting. Simply do not use for sensors which deliver values prior to
//    deep sleep.

// 3: Number of samples to take before doing a "real" boot. Decremented as samples are obtained
// 4: Reload value for (10). Needs to be applied by the firmware in the real boot (rtc_restart_samples_to_take())
//
// 5: FIFO location. First FIFO address in bits 0:7, first non-FIFO address in bits 8:15.
//                   Number of tag spaces in bits 16:23
// 6: Number of samples in FIFO.
// 7: FIFO tail (where next sample will be written. Increments by 1 for each sample)
// 8: FIFO head (where next sample will be read. Increments by 1 for each sample)
// 9: FIFO head timestamp. Used and maintained when pulling things off the FIFO. This is the timestamp of the
//    most recent sample pulled off; I.e. the head samples timestamp is this plus that sample's delta_t
// 10: FIFO tail timestamp. Used and maintained when adding things to the FIFO. This is the timestamp of the
//    most recent sample to have been added. I.e. a new sample's delta-t is calculated relative to this
// (9/10) are meaningless when (3) is zero
//

#define RTC_FIFO_BASE          10
#define RTC_FIFO_MAGIC         0x44695553

// RTCFIFO storage
#define RTC_FIFO_MAGIC_POS     (RTC_FIFO_BASE+0)
#define RTC_ALIGNMENT_POS      (RTC_FIFO_BASE+1)
#define RTC_TIMESTAMP_POS      (RTC_FIFO_BASE+2)

#define RTC_SAMPLESTOTAKE_POS  (RTC_FIFO_BASE+3)
#define RTC_SAMPLESPERBOOT_POS (RTC_FIFO_BASE+4)

#define RTC_FIFOLOC_POS        (RTC_FIFO_BASE+5)
#define RTC_FIFOCOUNT_POS      (RTC_FIFO_BASE+6)
#define RTC_FIFOTAIL_POS       (RTC_FIFO_BASE+7)
#define RTC_FIFOHEAD_POS       (RTC_FIFO_BASE+8)
#define RTC_FIFOTAIL_T_POS     (RTC_FIFO_BASE+9)
#define RTC_FIFOHEAD_T_POS     (RTC_FIFO_BASE+10)

// 32-127: FIFO space. Consisting of a number of tag spaces (see 4), followed by data entries.
//     Data entries consist of:
//     Bits 28:31  -> tag index. 0-15
//     Bits 25:27  -> decimals
//     Bits 16:24  -> delta-t in seconds from previous entry
//     Bits 0:15   -> sample value

#define RTC_DEFAULT_FIFO_START 32
#define RTC_DEFAULT_FIFO_END  128
#define RTC_DEFAULT_TAGCOUNT    5
#define RTC_DEFAULT_FIFO_LOC (RTC_DEFAULT_FIFO_START + (RTC_DEFAULT_FIFO_END<<8) + (RTC_DEFAULT_TAGCOUNT<<16))

#ifndef RTCTIME_SLEEP_ALIGNED
# define RTCTIME_SLEEP_ALIGNED rtc_time_deep_sleep_until_aligned
#endif

typedef struct
{
  uint32_t timestamp;
  uint32_t value;
  uint32_t decimals;
  uint32_t tag;
} sample_t;

static inline void rtc_fifo_clear_content(void);

static inline uint32_t rtc_fifo_get_tail(void)
{
  return rtc_mem_read(RTC_FIFOTAIL_POS);
}

static inline void rtc_fifo_put_tail(uint32_t val)
{
  rtc_mem_write(RTC_FIFOTAIL_POS,val);
}

static inline uint32_t rtc_fifo_get_head(void)
{
  return rtc_mem_read(RTC_FIFOHEAD_POS);
}

static inline void rtc_fifo_put_head(uint32_t val)
{
  rtc_mem_write(RTC_FIFOHEAD_POS,val);
}


static inline uint32_t rtc_fifo_get_tail_t(void)
{
  return rtc_mem_read(RTC_FIFOTAIL_T_POS);
}

static inline void rtc_fifo_put_tail_t(uint32_t val)
{
  rtc_mem_write(RTC_FIFOTAIL_T_POS,val);
}

static inline uint32_t rtc_fifo_get_head_t(void)
{
  return rtc_mem_read(RTC_FIFOHEAD_T_POS);
}

static inline void rtc_fifo_put_head_t(uint32_t val)
{
  rtc_mem_write(RTC_FIFOHEAD_T_POS,val);
}


static inline uint32_t rtc_fifo_get_count(void)
{
  return rtc_mem_read(RTC_FIFOCOUNT_POS);
}

static inline void rtc_fifo_put_count(uint32_t val)
{
  rtc_mem_write(RTC_FIFOCOUNT_POS,val);
}

static inline uint32_t rtc_fifo_get_tagcount(void)
{
  return (rtc_mem_read(RTC_FIFOLOC_POS)>>16)&0xff;
}

static inline uint32_t rtc_fifo_get_tagpos(void)
{
  return (rtc_mem_read(RTC_FIFOLOC_POS)>>0)&0xff;
}

static inline uint32_t rtc_fifo_get_last(void)
{
  return (rtc_mem_read(RTC_FIFOLOC_POS)>>8)&0xff;
}

static inline uint32_t rtc_fifo_get_first(void)
{
  return rtc_fifo_get_tagpos()+rtc_fifo_get_tagcount();
}

static inline void rtc_fifo_put_loc(uint32_t first, uint32_t last, uint32_t tagcount)
{
  rtc_mem_write(RTC_FIFOLOC_POS,first+(last<<8)+(tagcount<<16));
}

static inline uint32_t rtc_fifo_normalise_index(uint32_t index)
{
  if (index>=rtc_fifo_get_last())
    index=rtc_fifo_get_first();
  return index;
}

static inline void rtc_fifo_increment_count(void)
{
  rtc_fifo_put_count(rtc_fifo_get_count()+1);
}

static inline void rtc_fifo_decrement_count(void)
{
  rtc_fifo_put_count(rtc_fifo_get_count()-1);
}

static inline uint32_t rtc_get_samples_to_take(void)
{
  return rtc_mem_read(RTC_SAMPLESTOTAKE_POS);
}

static inline void rtc_put_samples_to_take(uint32_t val)
{
  rtc_mem_write(RTC_SAMPLESTOTAKE_POS,val);
}

static inline void rtc_decrement_samples_to_take(void)
{
  uint32_t stt=rtc_get_samples_to_take();
  if (stt)
    rtc_put_samples_to_take(stt-1);
}

static inline void rtc_restart_samples_to_take(void)
{
  rtc_put_samples_to_take(rtc_mem_read(RTC_SAMPLESPERBOOT_POS));
}

static inline uint32_t rtc_fifo_get_value(uint32_t entry)
{
  return entry&0xffff;
}

static inline uint32_t rtc_fifo_get_decimals(uint32_t entry)
{
  return (entry>>25)&0x07;
}

static inline uint32_t rtc_fifo_get_deltat(uint32_t entry)
{
  return (entry>>16)&0x1ff;
}

static inline uint32_t rtc_fifo_get_tagindex(uint32_t entry)
{
  return (entry>>28)&0x0f;
}

static inline uint32_t rtc_fifo_get_tag_from_entry(uint32_t entry)
{
  uint32_t index=rtc_fifo_get_tagindex(entry);
  uint32_t tags_at=rtc_fifo_get_tagpos();
  return rtc_mem_read(tags_at+index);
}

static inline void rtc_fifo_fill_sample(sample_t* dst, uint32_t entry, uint32_t timestamp)
{
  dst->timestamp=timestamp;
  dst->value=rtc_fifo_get_value(entry);
  dst->decimals=rtc_fifo_get_decimals(entry);
  dst->tag=rtc_fifo_get_tag_from_entry(entry);
}


// returns 1 if sample popped, 0 if not
static inline int8_t rtc_fifo_pop_sample(sample_t* dst)
{
  uint32_t count=rtc_fifo_get_count();

  if (count==0)
    return 0;
  uint32_t head=rtc_fifo_get_head();
  uint32_t timestamp=rtc_fifo_get_head_t();
  uint32_t entry=rtc_mem_read(head);
  timestamp+=rtc_fifo_get_deltat(entry);
  rtc_fifo_fill_sample(dst,entry,timestamp);

  head=rtc_fifo_normalise_index(head+1);

  rtc_fifo_put_head(head);
  rtc_fifo_put_head_t(timestamp);
  rtc_fifo_decrement_count();
  return 1;
}

// returns 1 if sample is available, 0 if not
static inline int8_t rtc_fifo_peek_sample(sample_t* dst, uint32_t from_top)
{
  if (rtc_fifo_get_count()<=from_top)
    return 0;
  uint32_t head=rtc_fifo_get_head();
  uint32_t entry=rtc_mem_read(head);
  uint32_t timestamp=rtc_fifo_get_head_t();
  timestamp+=rtc_fifo_get_deltat(entry);

  while (from_top--)
  {
    head=rtc_fifo_normalise_index(head+1);
    entry=rtc_mem_read(head);
    timestamp+=rtc_fifo_get_deltat(entry);
  }

  rtc_fifo_fill_sample(dst,entry,timestamp);
  return 1;
}

static inline void rtc_fifo_drop_samples(uint32_t from_top)
{
  uint32_t count=rtc_fifo_get_count();

  if (count<=from_top)
    from_top=count;
  uint32_t head=rtc_fifo_get_head();
  uint32_t head_t=rtc_fifo_get_head_t();

  while (from_top--)
  {
    uint32_t entry=rtc_mem_read(head);
    head_t+=rtc_fifo_get_deltat(entry);
    head=rtc_fifo_normalise_index(head+1);
    rtc_fifo_decrement_count();
  }
  rtc_fifo_put_head(head);
  rtc_fifo_put_head_t(head_t);
}

static inline int rtc_fifo_find_tag_index(uint32_t tag)
{
  uint32_t tags_at=rtc_fifo_get_tagpos();
  uint32_t count=rtc_fifo_get_tagcount();
  uint32_t i;

  for (i=0;i<count;i++)
  {
    uint32_t stag=rtc_mem_read(tags_at+i);

    if (stag==tag)
      return i;
    if (stag==0)
    {
      rtc_mem_write(tags_at+i,tag);
      return i;
    }
  }
  return -1;
}

static int32_t rtc_fifo_delta_t(uint32_t t, uint32_t ref_t)
{
  uint32_t delta=t-ref_t;
  if (delta>0x1ff)
    return -1;
  return delta;
}

static uint32_t rtc_fifo_construct_entry(uint32_t val, uint32_t tagindex, uint32_t decimals, uint32_t deltat)
{
  return (val & 0xffff) + ((deltat & 0x1ff) <<16) +
         ((decimals & 0x7)<<25) + ((tagindex & 0xf)<<28);
}

static inline void rtc_fifo_store_sample(const sample_t* s)
{
  uint32_t head=rtc_fifo_get_head();
  uint32_t tail=rtc_fifo_get_tail();
  uint32_t count=rtc_fifo_get_count();
  int32_t tagindex=rtc_fifo_find_tag_index(s->tag);

  if (count==0)
  {
    rtc_fifo_put_head_t(s->timestamp);
    rtc_fifo_put_tail_t(s->timestamp);
  }
  uint32_t tail_t=rtc_fifo_get_tail_t();
  int32_t deltat=rtc_fifo_delta_t(s->timestamp,tail_t);

  if (tagindex<0 || deltat<0)
  { // We got something that doesn't fit into the scheme. Might be a long delay, might
    // be some sort of dynamic change. In order to go on, we need to start over....
    // ets_printf("deltat is %d, tagindex is %d\n",deltat,tagindex);

    rtc_fifo_clear_content();
    rtc_fifo_put_head_t(s->timestamp);
    rtc_fifo_put_tail_t(s->timestamp);
    head=rtc_fifo_get_head();
    tail=rtc_fifo_get_tail();
    count=rtc_fifo_get_count();
    tagindex=rtc_fifo_find_tag_index(s->tag); // This should work now
    if (tagindex<0)
      return; // Uh-oh! This should never happen
  }

  if (head==tail && count>0)
  { // Full! Need to remove a sample
    sample_t dummy;
    rtc_fifo_pop_sample(&dummy);
  }

  rtc_mem_write(tail++,rtc_fifo_construct_entry(s->value,tagindex,s->decimals,deltat));
  rtc_fifo_put_tail(rtc_fifo_normalise_index(tail));
  rtc_fifo_put_tail_t(s->timestamp);
  rtc_fifo_increment_count();
}

static uint32_t rtc_fifo_make_tag(const uint8_t* s)
{
  uint32_t tag=0;
  int i;
  for (i=0;i<4;i++)
  {
    if (!s[i])
      break;
    tag+=((uint32_t)(s[i]&0xff))<<(i*8);
  }
  return tag;
}

static void rtc_fifo_tag_to_string(uint32_t tag, uint8_t s[5])
{
  int i;
  s[4]=0;
  for (i=0;i<4;i++)
    s[i]=(tag>>(8*i))&0xff;
}

static inline uint32_t rtc_fifo_get_divisor(const sample_t* s)
{
  uint8_t decimals=s->decimals;
  uint32_t div=1;
  while (decimals--)
    div*=10;
  return div;
}

static inline void rtc_fifo_clear_tags(void)
{
  uint32_t tags_at=rtc_fifo_get_tagpos();
  uint32_t count=rtc_fifo_get_tagcount();
  while (count--)
    rtc_mem_write(tags_at++,0);
}

static inline void rtc_fifo_clear_content(void)
{
  uint32_t first=rtc_fifo_get_first();
  rtc_fifo_put_tail(first);
  rtc_fifo_put_head(first);
  rtc_fifo_put_count(0);
  rtc_fifo_put_tail_t(0);
  rtc_fifo_put_head_t(0);
  rtc_fifo_clear_tags();
}

static inline void rtc_fifo_init(uint32_t first, uint32_t last, uint32_t tagcount)
{
  rtc_fifo_put_loc(first,last,tagcount);
  rtc_fifo_clear_content();
}

static inline void rtc_fifo_init_default(uint32_t tagcount)
{
  if (tagcount==0)
    tagcount=RTC_DEFAULT_TAGCOUNT;

  rtc_fifo_init(RTC_DEFAULT_FIFO_START,RTC_DEFAULT_FIFO_END,tagcount);
}


static inline uint8_t rtc_fifo_check_magic(void)
{
  if (rtc_mem_read(RTC_FIFO_MAGIC_POS)==RTC_FIFO_MAGIC)
    return 1;
  return 0;
}

static inline void rtc_fifo_set_magic(void)
{
  rtc_mem_write(RTC_FIFO_MAGIC_POS,RTC_FIFO_MAGIC);
}

static inline void rtc_fifo_unset_magic(void)
{
  rtc_mem_write(RTC_FIFO_MAGIC_POS,0);
}

static inline void rtc_fifo_deep_sleep_until_sample(uint32_t min_sleep_us)
{
  uint32_t align=rtc_mem_read(RTC_ALIGNMENT_POS);
  RTCTIME_SLEEP_ALIGNED(align,min_sleep_us);
}

static inline void rtc_fifo_prepare(uint32_t samples_per_boot, uint32_t us_per_sample, uint32_t tagcount)
{
  rtc_mem_write(RTC_SAMPLESPERBOOT_POS,samples_per_boot);
  rtc_mem_write(RTC_ALIGNMENT_POS,us_per_sample);

  rtc_put_samples_to_take(0);
  rtc_fifo_init_default(tagcount);
  rtc_fifo_set_magic();
}
#endif
