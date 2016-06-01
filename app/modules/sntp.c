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

// Module for Simple Network Time Protocol (SNTP)

#include "module.h"
#include "lauxlib.h"
#include "task/task.h"
#include "os_type.h"
#include "osapi.h"
#include "lwip/udp.h"
#include <stdlib.h>
#include "user_modules.h"
#include "lwip/dns.h"
#include "user_interface.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#ifdef LUA_USE_MODULES_RTCTIME
#include "rtc/rtctime.h"
#endif

#define NTP_PORT 123
#define NTP_ANYCAST_ADDR(dst)  IP4_ADDR(dst, 224, 0, 1, 1)

#define MAX_ATTEMPTS 5

#if 0
# define sntp_dbg(...) printf(__VA_ARGS__)
#else
# define sntp_dbg(...)
#endif

typedef enum {
  NTP_NO_ERR = 0,
  NTP_DNS_ERR,
  NTP_MEM_ERR,
  NTP_SEND_ERR,
  NTP_TIMEOUT_ERR
} ntp_err_t;

typedef struct
{
  uint32_t sec;
  uint32_t frac;
} ntp_timestamp_t;

typedef struct
{
  uint8_t mode : 3;
  uint8_t ver : 3;
  uint8_t LI : 2;
  uint8_t stratum;
  uint8_t poll;
  uint8_t precision;
  uint32_t root_delay;
  uint32_t root_dispersion;
  uint32_t refid;
  ntp_timestamp_t ref;
  ntp_timestamp_t origin;
  ntp_timestamp_t recv;
  ntp_timestamp_t xmit;
} ntp_frame_t;

typedef struct
{
  struct udp_pcb *pcb;
  ntp_timestamp_t cookie;
  os_timer_t timer;
  int sync_cb_ref;
  int err_cb_ref;
  uint8_t attempts;

  /* callback cache, locked by mutex */
  xSemaphoreHandle mtx;
  struct
  {
    ntp_err_t result;
    uint32_t  s, us;
    ip_addr_t used_server;
  } callback_args;
} sntp_state_t;

enum { TASKCMD_DOSEND, TASKCMD_TIMER, TASKCMD_CALLBACK };

#define LockCbArgs() xSemaphoreTake(state->mtx, portMAX_DELAY)
#define UnlockCbArgs() xSemaphoreGive(state->mtx)

static sntp_state_t *state;
static task_handle_t sntp_task;
static ip_addr_t server;

static void on_timeout (void *arg);

static void cleanup (lua_State *L)
{
  vSemaphoreDelete (state->mtx);
  os_timer_disarm (&state->timer);
  udp_remove (state->pcb);
  luaL_unref (L, LUA_REGISTRYINDEX, state->sync_cb_ref);
  luaL_unref (L, LUA_REGISTRYINDEX, state->err_cb_ref);
  free (state);
  state = 0;
}


static void handle_error (lua_State *L, ntp_err_t err)
{
  sntp_dbg("sntp: handle_error\n");
  if (state->err_cb_ref != LUA_NOREF)
  {
    lua_rawgeti (L, LUA_REGISTRYINDEX, state->err_cb_ref);
    lua_pushinteger (L, err);
    cleanup (L);
    lua_call (L, 1, 0);
  }
  else
    cleanup (L);
}


static inline void report_error (ntp_err_t err)
{
  LockCbArgs();
  state->callback_args.result = err;
  UnlockCbArgs();
  task_post_medium (sntp_task, TASKCMD_CALLBACK);
}


static void sntp_dosend (lua_State *L)
{
  if (state->attempts == 0)
  {
    os_timer_disarm (&state->timer);
    os_timer_setfn (&state->timer, on_timeout, NULL);
    os_timer_arm (&state->timer, 1000, 1);
  }

  ++state->attempts;
  sntp_dbg("sntp: attempt %d\n", state->attempts);

  struct pbuf *p = pbuf_alloc (PBUF_TRANSPORT, sizeof (ntp_frame_t), PBUF_RAM);
  if (!p)
  {
    handle_error (L, NTP_MEM_ERR);
    return;
  }

  ntp_frame_t req;
  os_memset (&req, 0, sizeof (req));
  req.ver = 4;
  req.mode = 3; // client
#ifdef LUA_USE_MODULES_RTCTIME
  struct rtc_timeval tv;
  rtctime_gettimeofday (&tv);
  req.xmit.sec = htonl (tv.tv_sec);
  req.xmit.frac = htonl (tv.tv_usec);
#else
  req.xmit.frac = htonl (system_get_time ());
#endif
  state->cookie = req.xmit;

  os_memcpy (p->payload, &req, sizeof (req));
  int ret = udp_sendto (state->pcb, p, &server, NTP_PORT);
  sntp_dbg("sntp: send: %d\n", ret);
  pbuf_free (p);
  if (ret != ERR_OK)
    handle_error (L, NTP_SEND_ERR);
}


static void sntp_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
  (void)arg;
  if (ipaddr == NULL)
  {
    sntp_dbg("DNS Fail!\n");
    report_error(NTP_DNS_ERR);
  }
  else
  {
    server = *ipaddr;
    task_post_medium (sntp_task, TASKCMD_DOSEND);
  }
}


static void on_timeout (void *arg)
{
  (void)arg;
  sntp_dbg("sntp: timer\n");
  task_post_medium (sntp_task, TASKCMD_TIMER);
}


static void on_recv (void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, uint16_t port)
{
  (void)port;
  sntp_dbg("sntp: on_recv\n");

  lua_State *L = arg;

  if (!state || state->pcb != pcb)
  {
    // "impossible", but don't leak if it did happen somehow...
    udp_remove (pcb);
    pbuf_free (p);
    return;
  }

  if (!p)
    return;

  if (p->len < sizeof (ntp_frame_t))
  {
    pbuf_free (p);
    return; // not an ntp frame, ignore
  }

  // make sure we have an aligned copy to work from
  ntp_frame_t ntp;
  os_memcpy (&ntp, p->payload, sizeof (ntp));
  pbuf_free (p);
  sntp_dbg("sntp: transmit timestamp: %u, %u\n", ntp.xmit.sec, ntp.xmit.frac);

  // sanity checks before we touch our clocks
  ip_addr_t anycast;
  NTP_ANYCAST_ADDR(&anycast);
  if (server.addr != anycast.addr && server.addr != addr->addr)
    return; // unknown sender, ignore

  if (ntp.origin.sec  != state->cookie.sec ||
      ntp.origin.frac != state->cookie.frac)
    return; // unsolicited message, ignore

  if (ntp.LI == 3)
    return; // server clock not synchronized (why did it even respond?!)

  server.addr = addr->addr;
  ntp.origin.sec  = ntohl (ntp.origin.sec);
  ntp.origin.frac = ntohl (ntp.origin.frac);
  ntp.recv.sec  = ntohl (ntp.recv.sec);
  ntp.recv.frac = ntohl (ntp.recv.frac);
  ntp.xmit.sec  = ntohl (ntp.xmit.sec);
  ntp.xmit.frac = ntohl (ntp.xmit.frac);

  const uint32_t UINT32_MAXI = (uint32_t)-1;
  const uint64_t MICROSECONDS = 1000000ull;
  const uint32_t NTP_TO_UNIX_EPOCH = 2208988800ul;

  bool have_cb = (state->sync_cb_ref != LUA_NOREF);

  // if we have rtctime, do higher resolution delta calc, else just use
  // the transmit timestamp
#ifdef LUA_USE_MODULES_RTCTIME
  struct rtc_timeval tv;

  rtctime_gettimeofday (&tv);
  ntp_timestamp_t dest;
  dest.sec = tv.tv_sec;
  dest.frac = (MICROSECONDS * tv.tv_usec) / UINT32_MAXI;

  // Compensation as per RFC2030
  int64_t delta_s = (((int64_t)ntp.recv.sec - ntp.origin.sec) +
                     ((int64_t)ntp.xmit.sec - dest.sec)) / 2;

  int64_t delta_f = (((int64_t)ntp.recv.frac - ntp.origin.frac) +
                     ((int64_t)ntp.xmit.frac - dest.frac)) / 2;

  dest.sec += delta_s;
  if (delta_f + dest.frac < 0)
  {
    delta_f += UINT32_MAXI;
    --dest.sec;
  }
  else if (delta_f + dest.frac > UINT32_MAXI)
  {
    delta_f -= UINT32_MAXI;
    ++dest.sec;
  }
  dest.frac += delta_f;

  tv.tv_sec = dest.sec - NTP_TO_UNIX_EPOCH;
  tv.tv_usec = (MICROSECONDS * dest.frac) / UINT32_MAXI;
  rtctime_settimeofday (&tv);

  if (have_cb)
  {
    LockCbArgs();
    state->callback_args.s = tv.tv_sec;
    state->callback_args.us = tv.tv_usec;
    UnlockCbArgs();
  }
#else
  if (have_cb)
  {
    LockCbArgs();
    state->callback_args.s = ntp.xmit.sec - NTP_TO_UNIX_EPOCH;
    state->callback_args.us = (MICROSECONDS * ntp.xmit.frac) / UINT32_MAXI;
    UnlockCbArgs();
  }
#endif

  if (have_cb)
  {
    LockCbArgs();
    state->callback_args.used_server = server;
    UnlockCbArgs();
    task_post_medium (sntp_task, TASKCMD_CALLBACK);
  }
}


static void run_task (task_param_t cmd, task_prio_t prio)
{
  (void)prio;
  lua_State *L = lua_getstate ();
  switch (cmd)
  {
    case TASKCMD_DOSEND:
      sntp_dosend (L);
      break;
    case TASKCMD_TIMER:
      if (state->attempts >= MAX_ATTEMPTS)
        handle_error (L, NTP_TIMEOUT_ERR);
      else
        sntp_dosend (L);
      break;
    case TASKCMD_CALLBACK:
    {
      LockCbArgs();
      ntp_err_t err = state->callback_args.result;
      UnlockCbArgs();
      if (err == NTP_NO_ERR)
      {
        bool have_cb = (state->sync_cb_ref != LUA_NOREF);
        if (have_cb)
        {
          lua_rawgeti (L, LUA_REGISTRYINDEX, state->sync_cb_ref);
          LockCbArgs();
          lua_pushnumber (L, state->callback_args.s);
          lua_pushnumber (L, state->callback_args.us);
          lua_pushstring (L, ipaddr_ntoa (&state->callback_args.used_server));
          UnlockCbArgs();
        }

        cleanup (L);

        if (have_cb)
          lua_call (L, 3, 0);
      }
      else
        handle_error (L, err);
    }
  }
}

// sntp.sync (server or nil, syncfn or nil, errfn or nil)
static int sntp_sync (lua_State *L)
{
  const char *errmsg = 0;
  #define sync_err(x) do { errmsg = x; goto error; } while (0)

  if (!sntp_task)
    sntp_task = task_get_id (run_task);
  if (!sntp_task)
    sync_err ("can't register task");

  // default to anycast address, then allow last server to stick
  if (server.addr == IPADDR_ANY)
    NTP_ANYCAST_ADDR(&server);

  if (state)
    return luaL_error (L, "sync in progress");

  state = (sntp_state_t *)malloc (sizeof (sntp_state_t));
  if (!state)
    sync_err ("out of memory");

  memset (state, 0, sizeof (sntp_state_t));

  state->mtx = xSemaphoreCreateMutex ();
  if (!state->mtx)
    sync_err ("no memory for mutex");

  state->sync_cb_ref = LUA_NOREF;
  state->err_cb_ref = LUA_NOREF;

  state->pcb = udp_new ();
  if (!state->pcb)
    sync_err ("out of memory");

  if (udp_bind (state->pcb, IP_ADDR_ANY, 0) != ERR_OK)
    sync_err ("no port available");

  udp_recv (state->pcb, on_recv, L);

  if (!lua_isnoneornil (L, 2))
  {
    lua_pushvalue (L, 2);
    state->sync_cb_ref = luaL_ref (L, LUA_REGISTRYINDEX);
  }
  else
    state->sync_cb_ref = LUA_NOREF;

  if (!lua_isnoneornil (L, 3))
  {
    lua_pushvalue (L, 3);
    state->err_cb_ref = luaL_ref (L, LUA_REGISTRYINDEX);
  }
  else
    state->err_cb_ref = LUA_NOREF;

  state->attempts = 0;

  // use last server, unless new one specified
  if (!lua_isnoneornil (L, 1))
  {
    size_t l;
    const char *hostname = luaL_checklstring(L, 1, &l);
    if (l>128 || hostname == NULL)
      sync_err("need <128 hostname");
    err_t err = dns_gethostbyname(hostname, &server, sntp_dns_found, state);
    if (err == ERR_INPROGRESS)
      return 0;  // Callback function sntp_dns_found will handle sntp_dosend for us
    else if (err == ERR_ARG)
      sync_err("bad hostname");
  }

  sntp_dosend (L);
  return 0;

error:
  if (state)
  {
    if (state->pcb)
      udp_remove (state->pcb);
    free (state);
    state = 0;
  }
  return luaL_error (L, errmsg);
}


// Module function map
static const LUA_REG_TYPE sntp_map[] = {
  { LSTRKEY("sync"),  LFUNCVAL(sntp_sync)  },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(SNTP, "sntp", sntp_map, NULL);
