/*
 * Copyright 2015 Dius Computing Pty Ltd. All rights reserved.
 * Copyright 2020 Nathaniel Wesley Filardo. All rights reserved.
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
 * @author Nathaniel Wesley Filardo <nwfilardo@gmail.com>
 */

// Module for Simple Network Time Protocol (SNTP) packet processing;
// see lua_modules/sntp/sntp.lua for the user-friendly bits of this.

#include "module.h"
#include "lauxlib.h"
#include "os_type.h"
#include "osapi.h"
#include "lwip/udp.h"
#include <stdlib.h>
#include "user_modules.h"
#include "lwip/dns.h"

#define max(a,b) ((a < b) ? b : a)

#define NTP_PORT 123
#define NTP_ANYCAST_ADDR(dst)  IP4_ADDR(dst, 224, 0, 1, 1)

#if 0
# define sntppkt_dbg(...) printf(__VA_ARGS__)
#else
# define sntppkt_dbg(...)
#endif

typedef struct
{
  uint32_t sec;
  uint32_t frac;
} ntp_timestamp_t;

static const uint32_t NTP_TO_UNIX_EPOCH = 2208988800ul;

typedef struct
{
  uint8_t mode : 3;
  uint8_t ver : 3;
  uint8_t LI : 2;
  uint8_t stratum;
  uint8_t poll;
  uint8_t precision;
  uint32_t delta_r;
  uint32_t epsilon_r;
  uint32_t refid;
  ntp_timestamp_t ref;
  ntp_timestamp_t origin;
  ntp_timestamp_t recv;
  ntp_timestamp_t xmit;
} __attribute__((packed)) ntp_frame_t;

#define NTP_RESPONSE_METATABLE  "sntppkt.resp"
typedef struct {
    /* Copied from incoming packet */
    uint32_t  delta_r;
    uint32_t  epsilon_r;
    uint8_t   LI;
    uint8_t   stratum;

    /* Computed as per RFC 5905; units are 2^(-32) seconds */
    int64_t   theta;
    int64_t   delta;

    /* Local computation */
    uint64_t  t_rx;
    uint32_t  score;
} ntp_response_t;

static const uint32_t MICROSECONDS      = 1000000;

static uint64_t
sntppkt_div1m(uint64_t n) {
  uint64_t q1 = (n >> 5) + (n >> 10);
  uint64_t q2 = (n >> 12) + (q1 >> 1);
  uint64_t q3 = (q2 >> 11) - (q2 >> 23);

  uint64_t q = n + q1 + q2 - q3;

  q = q >> 20;

  // Ignore the error term -- it is measured in pico seconds
  return q;
}

static uint32_t
sntppkt_us_to_frac(uint64_t us) {
  return sntppkt_div1m(us << 32);
}

static uint32_t
sntppkt_long_s(uint64_t time) {
  return time >> 32;
}

static uint32_t
sntppkt_long_us(uint64_t time) {
  return ((time & 0xFFFFFFFF) * MICROSECONDS) >> 32;
}

/*
 * Convert sec/usec to a Lua string suitable for depositing into a SNTP packet
 * buffer.  This is a little gross, but it's not the worst thing a C
 * programmer's ever done, I'm sure.
 */
static int
sntppkt_make_ts(lua_State *L) {
  ntp_timestamp_t ts;

  ts.sec  = htonl(luaL_checkinteger(L, 1) + NTP_TO_UNIX_EPOCH) ;

  uint32_t usec = luaL_checkinteger(L, 2)  ;
  ts.frac = htonl(sntppkt_us_to_frac(usec));

  lua_pushlstring(L, (const char *)&ts, sizeof(ts));
  return 1;
}

/*
 * Convert ntp_timestamp_t to uint64_t
 */
static uint64_t
sntppkt_ts2uint64(ntp_timestamp_t ts) {
  return (((uint64_t)(ts.sec)) << 32) + (uint64_t)(ts.frac);
}

/*
 * Process a SNTP packet as contained in a Lua string, given a cookie timestamp
 * and local clock second*usecond pair.  Generates a ntp_response_t userdata
 * for later processing or a string if the server is telling us to go away.
 *
 * :: string (packet)
 * -> string (cookie)
 * -> int (local clock, sec component)
 * -> int (local clock, usec component)
 * -> sntppkt.resp
 *
 */
static int
sntppkt_proc_pkt(lua_State *L) {
  size_t       pkts_len;
  ntp_frame_t  pktb;
  const char  *pkts = lua_tolstring(L, 1, &pkts_len);

  luaL_argcheck(L, pkts && pkts_len == sizeof(pktb), 1, "Bad packet");

  // make sure we have an aligned copy to work from
  memcpy (&pktb, pkts, sizeof(pktb));

  uint32_t now_sec  = luaL_checkinteger(L, 3);
  uint32_t now_usec = luaL_checkinteger(L, 4);

  size_t           cookie_len;
  ntp_timestamp_t *cookie = (ntp_timestamp_t*) lua_tolstring(L, 2, &cookie_len);

  /* Bad *length* is clearly something bogus */
  luaL_argcheck(L, cookie && cookie_len == sizeof(*cookie), 2, "Bad cookie");

  /* But mismatching value might just be a packet caught in the crossfire */
  if (memcmp((const char *)cookie, (const char *)&pktb.origin, sizeof (*cookie))) {
    /* bad cookie; return nil */
    return 0;
  }

  /* KOD?  Do this test *after* checking the cookie */
  if (pktb.LI == 3) {
    lua_pushlstring(L, (const char *)&pktb.refid, 4);
    return 1;
  }

  ntp_response_t *ntpr = lua_newuserdata(L, sizeof(ntp_response_t));
  luaL_getmetatable(L, NTP_RESPONSE_METATABLE);
  lua_setmetatable(L, -2);

  ntpr->LI        = pktb.LI;
  ntpr->stratum   = pktb.stratum;

  // NTP Short Format: 16 bit seconds, 16 bit fraction
  ntpr->delta_r   = ntohl(pktb.delta_r);
  ntpr->epsilon_r = ntohl(pktb.epsilon_r);

  /* Heavy time lifting time */

  // NTP Long Format: 32 bit seconds, 32 bit fraction
  pktb.origin.sec  = ntohl(pktb.origin.sec);
  pktb.origin.frac = ntohl(pktb.origin.frac);
  pktb.recv.sec    = ntohl(pktb.recv.sec);
  pktb.recv.frac   = ntohl(pktb.recv.frac);
  pktb.xmit.sec    = ntohl(pktb.xmit.sec);
  pktb.xmit.frac   = ntohl(pktb.xmit.frac);

  uint64_t ntp_origin = sntppkt_ts2uint64(pktb.origin); // We sent it
  uint64_t ntp_recv   = sntppkt_ts2uint64(pktb.recv);   // They got it
  uint64_t ntp_xmit   = sntppkt_ts2uint64(pktb.xmit);   // They replied

  // When we got it back (our clock)
  uint64_t ntp_dest   = (((uint64_t) now_sec + NTP_TO_UNIX_EPOCH ) << 32)
                        + sntppkt_us_to_frac(now_usec);

  ntpr->t_rx = ntp_dest;

              // |       outgoing offset           |   |        incoming offset         |
  ntpr->theta = (int64_t)(ntp_recv - ntp_origin) / 2 + (int64_t)(ntp_xmit - ntp_dest) / 2;

              // |       our clock delta           |   |       their clock delta        |
  ntpr->delta = (int64_t)(ntp_dest - ntp_origin) / 2 + (int64_t)(ntp_xmit - ntp_recv) / 2;

  /* Used by sntppkt_resp_pick; bias towards closer clocks */
  ntpr->score = ntpr->delta_r * 2 + ntpr->delta;

  sntppkt_dbg("SNTPPKT n_o=%llx n_r=%llx n_x=%llx n_d=%llx th=%llx d=%llx\n",
    ntp_origin, ntp_recv, ntp_xmit, ntp_dest, ntpr->theta, ntpr->delta);

  return 1;
}

/*
 * Left-biased selector of a "preferred" NTP response.  Note that preference
 * is rather subjective!
 *
 * Returns true iff we'd prefer the second response to the first.
 *
 * :: sntppkt.resp -> sntppkt.resp -> boolean
 */

static int
sntppkt_resp_pick(lua_State *L) {
  ntp_response_t *a = luaL_checkudata(L, 1, NTP_RESPONSE_METATABLE);
  ntp_response_t *b = luaL_checkudata(L, 2, NTP_RESPONSE_METATABLE);
  int biased = 0;

  biased = lua_toboolean(L, 3);

  /*
   * If we're "biased", prefer the second structure if the score is less than
   * 3/4ths of the score of the first.  An unbiased comparison just uses the
   * raw score values.
   */
  if (biased) {
    lua_pushboolean(L, a->score * 3 > b->score * 4);
  } else {
    lua_pushboolean(L, a->score     > b->score    );
  }
  return 1;
}

static void
field_from_integer(lua_State *L, const char * field_name, lua_Integer value) {
  lua_pushinteger(L, value);
  lua_setfield(L, -2, field_name);
}

/*
 * Inflate a NTP response into a Lua table
 *
 * :: sntppkt.resp -> { }
 */
static int
sntppkt_resp_totable(lua_State *L) {
  ntp_response_t *r = luaL_checkudata(L, 1, NTP_RESPONSE_METATABLE);

  lua_createtable(L, 0, 6);

  sntppkt_dbg("SNTPPKT READ th=%llx\n", r->theta);

  /*
   * The raw response ends up in here, too, so that we can pass the
   * tabular view to user callbacks and still have the internal large integers
   * for use in drift_compensate.
   */
  lua_pushvalue(L, 1);
  lua_setfield(L, -2, "raw");

  field_from_integer(L, "theta_s",  (int32_t)sntppkt_long_s (r->theta));
  field_from_integer(L, "theta_us", sntppkt_long_us(r->theta));

  field_from_integer(L, "delta", r->delta >> 16);
  field_from_integer(L, "delta_r", r->delta_r);
  field_from_integer(L, "epsilon_r", r->epsilon_r);

  field_from_integer(L, "leapind", r->LI);
  field_from_integer(L, "stratum", r->stratum);
  field_from_integer(L, "rx_s"   , sntppkt_long_s (r->t_rx) - NTP_TO_UNIX_EPOCH);
  field_from_integer(L, "rx_us"  , sntppkt_long_us(r->t_rx));

  return 1;
}

/*
 * Compute local RTC drift rate given a SNTP response, previous sample time,
 * and error integral value (i.e., this is a PI controller).  Returns new rate
 * and integral value.
 *
 * Likely only sensible if resp->theta is sufficiently small (so we require
 * less than a quarter of a second) and the inter-sample duration must, of
 * course, be positive.
 *
 * There's nothing magic about the constants of the PI loop here; someone with
 * a better understanding of control theory is welcome to suggest improvements.
 *
 * :: sntppkt.resp (most recent sample)
 * -> sntppkt.resp (prior sample)
 * -> int (integral)
 * -> int (rate), int (integral)
 *
 */
static int
sntppkt_resp_drift_compensate(lua_State *L) {
  ntp_response_t *resp = luaL_checkudata(L, 1, NTP_RESPONSE_METATABLE);

  int32_t tsec = sntppkt_long_s(resp->theta);
  uint32_t tfrac = resp->theta & 0xFFFFFFFF;
  if ((tsec != 0 && tsec != -1) ||
      ((tsec ==  0) && (tfrac > 0x40000000UL)) ||
      ((tsec == -1) && (tfrac < 0xC0000000UL))) {
    return luaL_error(L, "Large deviation");
  }

  int32_t dsec = sntppkt_long_s(resp->delta);
  if (dsec != 0 && dsec != -1) {
    return luaL_error(L, "Large estimated error");
  }

  ntp_response_t *prior_resp = luaL_checkudata(L, 2, NTP_RESPONSE_METATABLE);

  int64_t  isdur    = resp->t_rx - prior_resp->t_rx;
  if (isdur <= 0) {
    return luaL_error(L, "Negative time base");
  }

  int32_t  err_int  = luaL_checkinteger(L, 3);

  /* P: Compute drift over 2, in parts per 2^32 as expected by the RTC. */
  int32_t drift2 = (resp->theta << 31) / isdur;

  /* PI: rate is drift/4 + integral error */
  int32_t newrate = (drift2 >> 1) + err_int;

  /* I: Adjust integral by drift/32, with a little bit of wind-up protection */
  if ((newrate > ~0x40000) && (newrate < 0x40000)) {
    err_int += (drift2 + 0xF) >> 4;
  }

  sntppkt_dbg("SNTPPKT drift: isdur=%llx drift2=%lx -> newrate=%lx err_int=%lx\n",
              isdur, drift2, newrate, err_int);

  lua_pushinteger(L, newrate);
  lua_pushinteger(L, err_int);
  return 2;
}

LROT_BEGIN(sntppkt_resp, NULL, LROT_MASK_INDEX)
  LROT_TABENTRY( __index, sntppkt_resp )
  LROT_FUNCENTRY( pick, sntppkt_resp_pick )
  LROT_FUNCENTRY( totable, sntppkt_resp_totable )
  LROT_FUNCENTRY( drift_compensate, sntppkt_resp_drift_compensate )
LROT_END(sntppkt_resp, NULL, LROT_MASK_INDEX)

static int
sntppkt_init(lua_State *L)
{
  luaL_rometatable(L, NTP_RESPONSE_METATABLE, LROT_TABLEREF(sntppkt_resp));
  return 0;
}

// Module function map
LROT_BEGIN(sntppkt, NULL, 0)
  LROT_FUNCENTRY( make_ts  , sntppkt_make_ts   )
  LROT_FUNCENTRY( proc_pkt , sntppkt_proc_pkt  )
LROT_END( sntppkt, NULL, 0 )

NODEMCU_MODULE(SNTPPKT, "sntppkt", sntppkt, sntppkt_init);
