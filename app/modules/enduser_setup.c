/*
 * Copyright 2015 Robert Foss. All rights reserved.
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
 * @author Robert Foss <dev@robertfoss.se>
 *
 * Additions & fixes: Johny Mattsson <jmattsson@dius.com.au>
 *                    Jason Follas <jfollas@gmail.com>
 */

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ctype.h"
#include "user_interface.h"
#include "espconn.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "vfs.h"
#include "task/task.h"

/* Set this to 1 to generate debug messages. Uses debug callback provided by Lua. Example: enduser_setup.start(successFn, print, print) */
#define ENDUSER_SETUP_DEBUG_ENABLE 0

/* Set this to 1 to output the contents of HTTP requests when debugging. Useful if you need it, but can get pretty noisy */
#define ENDUSER_SETUP_DEBUG_SHOW_HTTP_REQUEST 0


#define MIN(x, y)  (((x) < (y)) ? (x) : (y))
#define LITLEN(strliteral) (sizeof (strliteral) -1)
#define STRINGIFY(x) #x
#define NUMLEN(x) (sizeof(STRINGIFY(x)) - 1)

#define ENDUSER_SETUP_ERR_FATAL        (1 << 0)
#define ENDUSER_SETUP_ERR_NONFATAL     (1 << 1)
#define ENDUSER_SETUP_ERR_NO_RETURN    (1 << 2)

#define ENDUSER_SETUP_ERR_OUT_OF_MEMORY          1
#define ENDUSER_SETUP_ERR_CONNECTION_NOT_FOUND   2
#define ENDUSER_SETUP_ERR_UNKOWN_ERROR           3
#define ENDUSER_SETUP_ERR_SOCKET_ALREADY_OPEN    4
#define ENDUSER_SETUP_ERR_MAX_NUMBER             5
#define ENDUSER_SETUP_ERR_ALREADY_INITIALIZED    6

/**
 * DNS Response Packet:
 *
 * |DNS ID - 16 bits|
 * |dns_header|
 * |QNAME|
 * |dns_body|
 * |ip - 32 bits|
 *
 *        DNS Header Part          |  FLAGS | | Q COUNT |  | A CNT  |  |AUTH CNT|  | ADD CNT| */
static const char dns_header[] = { 0x80, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 };
/*        DNS Query Part           | Q TYPE |  | Q CLASS| */
static const char dns_body[]   = { 0x00, 0x01, 0x00, 0x01,
/*        DNS Answer Part          |LBL OFFS|  |  TYPE  |  |  CLASS |  |         TTL        |  | RD LEN | */
                                   0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x04 };

static const char http_html_gz_filename[] = "enduser_setup.html.gz";
static const char http_html_filename[] = "enduser_setup.html";
static const char http_header_200[] = "HTTP/1.1 200 OK\r\nCache-control:no-cache\r\nConnection:close\r\nContent-Type:text/html; charset=utf-8\r\n"; /* Note single \r\n here! */
static const char http_header_204[] = "HTTP/1.1 204 No Content\r\nContent-Length:0\r\nConnection:close\r\n\r\n";
static const char http_header_302[] = "HTTP/1.1 302 Moved\r\nLocation: http://nodemcu.portal/\r\nContent-Length:0\r\nConnection:close\r\n\r\n";
static const char http_header_302_trying[] = "HTTP/1.1 302 Moved\r\nLocation: /?trying=true\r\nContent-Length:0\r\nConnection:close\r\n\r\n";
static const char http_header_400[] = "HTTP/1.1 400 Bad request\r\nContent-Length:0\r\nConnection:close\r\n\r\n";
static const char http_header_404[] = "HTTP/1.1 404 Not found\r\nContent-Length:10\r\nConnection:close\r\n\r\nNot found\n";
static const char http_header_405[] = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length:0\r\nConnection:close\r\n\r\n";
static const char http_header_500[] = "HTTP/1.1 500 Internal Error\r\nContent-Length:6\r\nConnection:close\r\n\r\nError\n";

static const char http_header_content_len_fmt[] = "Content-length:%5d\r\n\r\n";
static const char http_html_gzip_contentencoding[] = "Content-Encoding: gzip\r\n";

/* Externally defined: static const char enduser_setup_html_default[] = ... */
#include "enduser_setup/enduser_setup.html.gz.def.h"

// The tcp_arg can be either a pointer to the scan_listener_t or http_request_buffer_t.
// The enum defines which one it is.
typedef enum {
  SCAN_LISTENER_STRUCT_TYPE = 1,
  HTTP_REQUEST_BUFFER_STRUCT_TYPE = 2
} struct_type_t;

typedef struct {
  struct_type_t struct_type;
} tcp_arg_t;

typedef struct scan_listener
{
  struct_type_t struct_type;
  struct tcp_pcb *conn;
  struct scan_listener *next;
} scan_listener_t;

typedef struct {
  struct_type_t struct_type;
  size_t length;
  char data[0];
} http_request_buffer_t;

typedef struct
{
  struct espconn *espconn_dns_udp;
  struct tcp_pcb *http_pcb;
  char *http_payload_data;
  uint32_t http_payload_len;
  char *ap_ssid;
  os_timer_t check_station_timer;
  os_timer_t shutdown_timer;
  int lua_connected_cb_ref;
  int lua_err_cb_ref;
  int lua_dbg_cb_ref;
  scan_listener_t *scan_listeners;
  uint8_t softAPchannel;
  uint8_t success;
  uint8_t callbackDone;
  uint8_t lastStationStatus;
  uint8_t connecting;
} enduser_setup_state_t;

static enduser_setup_state_t *state;

static bool manual = false;
static task_handle_t do_station_cfg_handle;

static int enduser_setup_manual(lua_State* L);
static int enduser_setup_start(lua_State* L);
static int enduser_setup_stop(lua_State* L);
static void enduser_setup_stop_callback(void *ptr);
static void enduser_setup_station_start(void);
static void enduser_setup_ap_start(void);
static void enduser_setup_ap_stop(void);
static void enduser_setup_check_station(void *p);
static void enduser_setup_debug(int line, const char *str);

static char ipaddr[16];

#if ENDUSER_SETUP_DEBUG_ENABLE
#define ENDUSER_SETUP_DEBUG(str) enduser_setup_debug(__LINE__, str)
#else
#define ENDUSER_SETUP_DEBUG(str) do {} while(0)
#endif


#define ENDUSER_SETUP_ERROR(str, err, err_severity) \
  do { \
    ENDUSER_SETUP_DEBUG(str); \
    if (err_severity & ENDUSER_SETUP_ERR_FATAL) enduser_setup_stop(lua_getstate());\
      enduser_setup_error(__LINE__, str, err);\
    if (!(err_severity & ENDUSER_SETUP_ERR_NO_RETURN)) \
      return err; \
  } while (0)


#define ENDUSER_SETUP_ERROR_VOID(str, err, err_severity) \
  do { \
    ENDUSER_SETUP_DEBUG(str); \
    if (err_severity & ENDUSER_SETUP_ERR_FATAL) enduser_setup_stop(lua_getstate());\
       enduser_setup_error(__LINE__, str, err);\
    if (!(err_severity & ENDUSER_SETUP_ERR_NO_RETURN)) \
      return; \
  } while (0)


static void enduser_setup_debug(int line, const char *str)
{
  lua_State *L = lua_getstate();
  if(state != NULL && state->lua_dbg_cb_ref != LUA_NOREF)
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, state->lua_dbg_cb_ref);
    lua_pushfstring(L, "%d: \t%s", line, str);
    luaL_pcallx(L, 1, 0);
  }
}


static void enduser_setup_error(int line, const char *str, int err)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_error");

  lua_State *L = lua_getstate();
  if (state != NULL && state->lua_err_cb_ref != LUA_NOREF)
  {
    lua_rawgeti (L, LUA_REGISTRYINDEX, state->lua_err_cb_ref);
    lua_pushinteger(L, err);
    lua_pushfstring(L, "%d: \t%s", line, str);
    luaL_pcallx (L, 2, 0);
  }
}


static void enduser_setup_connected_callback()
{
  ENDUSER_SETUP_DEBUG("enduser_setup_connected_callback");

  lua_State *L = lua_getstate();
  if (state != NULL && state->lua_connected_cb_ref != LUA_NOREF)
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, state->lua_connected_cb_ref);
    luaL_pcallx(L, 0, 0);
  }
}

#include "pm/swtimer.h"
static void enduser_setup_check_station_start(void)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_check_station_start");

  os_timer_setfn(&(state->check_station_timer), enduser_setup_check_station, NULL);
  SWTIMER_REG_CB(enduser_setup_check_station, SWTIMER_RESUME);
    //The function enduser_setup_check_station checks for a successful connection to the configured AP
    //My guess: I'm not sure about whether or not user feedback is given via the web interface, but I don't see a problem with letting this timer resume.
  os_timer_arm(&(state->check_station_timer), 3*1000, TRUE);
}


static void enduser_setup_check_station_stop(void)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_check_station_stop");

  if (state != NULL)
  {
    os_timer_disarm(&(state->check_station_timer));
  }
}


/**
 * Check Station
 *
 * Check that we've successfully entered station mode.
 */
static void enduser_setup_check_station(void *p)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_check_station");

  (void)p;
  struct ip_info ip;
  memset(&ip, 0, sizeof(struct ip_info));

  wifi_get_ip_info(STATION_IF, &ip);

  int i;
  char has_ip = 0;
  for (i = 0; i < sizeof(struct ip_info); ++i)
  {
    has_ip |= ((char *) &ip)[i];
  }

  uint8_t currChan = wifi_get_channel();

  if (has_ip == 0)
  {
    /* No IP Address yet, so check the reported status */
    uint8_t curr_status = wifi_station_get_connect_status();
    char buf[20];
    sprintf(buf, "status=%d,chan=%d", curr_status, currChan);
    ENDUSER_SETUP_DEBUG(buf);

    if (curr_status == 2 || curr_status == 3 || curr_status == 4)
    {
      state->connecting = 0;

      /* If the status is an error status and the channel changed, then cache the
       * status to state since the Station won't be able to report the same status
       * after switching the channel back to the SoftAP's
       */
      if (currChan != state->softAPchannel) {
        state->lastStationStatus = curr_status;

        ENDUSER_SETUP_DEBUG("Turning off Station due to different channel than AP");

        wifi_station_disconnect();
        wifi_set_opmode(SOFTAP_MODE);
        enduser_setup_ap_start();
      }
   }
   return;
  }

  sprintf (ipaddr, "%d.%d.%d.%d", IP2STR(&ip.ip.addr));

  state->success = 1;
  state->lastStationStatus = 5; /*  We have an IP Address, so the status is 5 (as of SDK 1.5.1) */
  state->connecting = 0;

#if ENDUSER_SETUP_DEBUG_ENABLE
  char debuginfo[100];
  sprintf(debuginfo, "AP_CHAN: %d, STA_CHAN: %d", state->softAPchannel, currChan);
  ENDUSER_SETUP_DEBUG(debuginfo);
#endif

  if (currChan == state->softAPchannel)
  {
    enduser_setup_connected_callback();
    state->callbackDone = 1;
  }
  else
  {
    ENDUSER_SETUP_DEBUG("Turning off Station due to different channel than AP");
    wifi_station_disconnect();
    wifi_set_opmode(SOFTAP_MODE);
    enduser_setup_ap_start();
  }

  enduser_setup_check_station_stop();

  /* Trigger shutdown, but allow time for HTTP client to fetch last status. */
  if (!manual)
  {
    os_timer_setfn(&(state->shutdown_timer), enduser_setup_stop_callback, NULL);
    SWTIMER_REG_CB(enduser_setup_stop_callback, SWTIMER_RESUME);
      //The function enduser_setup_stop_callback frees services and resources used by enduser setup.
      //My guess: Since it would lead to a memory leak, it's probably best to resume this timer.
    os_timer_arm(&(state->shutdown_timer), 10*1000, FALSE);
  }
}


/* --- Connection closing handling ----------------------------------------- */

/* It is far more memory efficient to let the other end close the connection
 * first and respond to that, than us initiating the closing. The latter
 * seems to leave the pcb in a fin_wait state for a long time, which can
 * starve us of memory over time.
 *
 * By instead using the poll function to schedule a hard abort a few seconds
 * from now we achieve a deadline close. The downside is a (very) slight
 * risk of dropping the connection early, but in this application that's
 * hidden by the retries on the JavaScript side anyway.
 */


/* Callback on timeout to hard-close a connection */
static err_t force_abort (void *arg, struct tcp_pcb *pcb)
{
  ENDUSER_SETUP_DEBUG("force_abort");

  (void)arg;
  tcp_poll (pcb, 0, 0);
  tcp_abort (pcb);
  return ERR_ABRT;
}

/* Callback to detect a remote-close of a connection */
static err_t handle_remote_close (void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
  ENDUSER_SETUP_DEBUG("handle_remote_close");

  (void)arg; (void)err;
  if (p) /* server sent us data, just ACK and move on */
  {
    tcp_recved (pcb, p->tot_len);
    pbuf_free (p);
  }
  else /* hey, remote end closed, we can do a soft close safely, yay! */
  {
    tcp_recv (pcb, 0);
    tcp_poll (pcb, 0, 0);
    tcp_close (pcb);
  }
  return ERR_OK;
}

/* Set up a deferred close of a connection, as discussed above. */
static inline void deferred_close (struct tcp_pcb *pcb)
{
  ENDUSER_SETUP_DEBUG("deferred_close");

  tcp_poll (pcb, force_abort, 15); /* ~3sec from now */
  tcp_recv (pcb, handle_remote_close);
  tcp_sent (pcb, 0);
}

/* Convenience function to queue up a close-after-send.  */
static err_t close_once_sent (void *arg, struct tcp_pcb *pcb, u16_t len)
{
  ENDUSER_SETUP_DEBUG("close_once_sent");

  (void)arg; (void)len;
  deferred_close (pcb);
  return ERR_OK;
}

/* ------------------------------------------------------------------------- */

/**
 * Get length of param value
 *
 * This is being called with a fragment of the parameters passed in the
 * URL for GET requests or part of the body of a POST request.
 * The string will look like one of these
 * "SecretPassword HTTP/1.1"
 * "SecretPassword&wifi_ssid=..."
 * "SecretPassword"
 * The string is searched for the first occurence of deliemiter '&' or ' '.
 * If found return the length up to that position.
 * If not found return the length of the string.
 *
 */
static int enduser_setup_get_lenth_of_param_value(const char *str)
{
  char *found = strpbrk (str, "& ");
  if (!found)
  {
    return strlen(str);
  }
  else
  {
    return found - str;
  }
}

/**
 * Load HTTP Payload
 *
 * @return - 0 if payload loaded successfully
 *           1 if default html was loaded
 *           2 if out of memory
 */
static int enduser_setup_http_load_payload(void)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_http_load_payload");

  int err = VFS_RES_ERR;
  int err2 = VFS_RES_ERR;
  int file_len = 0;

  /* Try to open enduser_setup.html.gz from SPIFFS first */
  int f = vfs_open(http_html_gz_filename, "r");

  if (f)
  {
      err = vfs_lseek(f, 0, VFS_SEEK_END);
      file_len = (int) vfs_tell(f);
      err2 = vfs_lseek(f, 0, VFS_SEEK_SET);
  }

  if (!f || err == VFS_RES_ERR || err2 == VFS_RES_ERR)
  {
    if (f)
    {
      vfs_close(f);
    }

    /* If that didn't work, try to open enduser_setup.html from SPIFFS */
    f = vfs_open(http_html_filename, "r");

    if (f)
    {
        err = vfs_lseek(f, 0, VFS_SEEK_END);
        file_len = (int) vfs_tell(f);
        err2 = vfs_lseek(f, 0, VFS_SEEK_SET);
    }
  }

  char cl_hdr[30];
  size_t ce_len = 0;

  sprintf(cl_hdr, http_header_content_len_fmt, file_len);
  size_t cl_len = strlen(cl_hdr);

  if (!f || err == VFS_RES_ERR || err2 == VFS_RES_ERR)
  {
    ENDUSER_SETUP_DEBUG("Unable to load file enduser_setup.html, loading default HTML...");

    if (f)
    {
      vfs_close(f);
    }

    sprintf(cl_hdr, http_header_content_len_fmt, sizeof(enduser_setup_html_default));
    cl_len = strlen(cl_hdr);
    int html_len = LITLEN(enduser_setup_html_default);

    if (enduser_setup_html_default[0] == 0x1f && enduser_setup_html_default[1] == 0x8b)
    {
        ce_len = strlen(http_html_gzip_contentencoding);
        html_len = enduser_setup_html_default_len; /* Defined in enduser_setup/enduser_setup.html.gz.def.h by xxd -i */
        ENDUSER_SETUP_DEBUG("Content is gzipped");
    }

    int payload_len = LITLEN(http_header_200) + cl_len + ce_len + html_len;
    state->http_payload_len = payload_len;
    state->http_payload_data = (char *) malloc(payload_len);

    if (state->http_payload_data == NULL)
    {
      return 2;
    }

    int offset = 0;
    memcpy(&(state->http_payload_data[offset]), &(http_header_200), LITLEN(http_header_200));
    offset += LITLEN(http_header_200);

    if (ce_len > 0)
    {
        offset += sprintf(state->http_payload_data + offset, http_html_gzip_contentencoding, ce_len);
    }

    memcpy(&(state->http_payload_data[offset]), &(cl_hdr), cl_len);
    offset += cl_len;
    memcpy(&(state->http_payload_data[offset]), &(enduser_setup_html_default), sizeof(enduser_setup_html_default));

    return 1;
  }

  char magic[2];
  vfs_read(f, magic, 2);

  if (magic[0] == 0x1f && magic[1] == 0x8b)
  {
    ce_len = strlen(http_html_gzip_contentencoding);
    ENDUSER_SETUP_DEBUG("Content is gzipped");
  }

  int payload_len = LITLEN(http_header_200) + cl_len + ce_len + file_len;
  state->http_payload_len = payload_len;
  state->http_payload_data = (char *) malloc(payload_len);

  if (state->http_payload_data == NULL)
  {
    return 2;
  }

  vfs_lseek(f, 0, VFS_SEEK_SET);

  int offset = 0;

  memcpy(&(state->http_payload_data[offset]), &(http_header_200), LITLEN(http_header_200));
  offset += LITLEN(http_header_200);

  if (ce_len > 0)
  {
    offset += sprintf(state->http_payload_data + offset, http_html_gzip_contentencoding, ce_len);
  }

  memcpy(&(state->http_payload_data[offset]), &(cl_hdr), cl_len);
  offset += cl_len;
  vfs_read(f, &(state->http_payload_data[offset]), file_len);
  vfs_close(f);

  return 0;
}


/**
 * De-escape URL data
 *
 * Parse escaped and form encoded data of request.
 *
 * @return - return 0 if the HTTP parameter is decoded into a valid string.
 */
static int enduser_setup_http_urldecode(char *dst, const char *src, int src_len, int dst_len)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_http_urldecode");

  char *dst_start = dst;
  char *dst_last = dst + dst_len - 1; /* -1 to reserve space for last \0 */
  char a, b;
  int i;
  for (i = 0; i < src_len && *src && dst < dst_last; ++i)
  {
    if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b)))
    {
      if (a >= 'a')
      {
        a -= 'a'-'A';
      }
      if (a >= 'A')
      {
        a -= ('A' - 10);
      }
      else
      {
        a -= '0';
      }
      if (b >= 'a')
      {
        b -= 'a'-'A';
      }
      if (b >= 'A')
      {
        b -= ('A' - 10);
      }
      else
      {
        b -= '0';
      }
      *dst++ = 16 * a + b;
      src += 3;
      i += 2;
    } else {
      char c = *src++;
      if (c == '+')
      {
        c = ' ';
      }
      *dst++ = c;
    }
  }
  *dst++ = '\0';
  return (i < src_len); /* did we fail to process all the input? */
}


/**
 * Task to do the actual station configuration.
 * This config *cannot* be done in the network receive callback or serious
 * issues like memory corruption occur.
 */
static void do_station_cfg (task_param_t param, uint8_t prio)
{
  ENDUSER_SETUP_DEBUG("do_station_cfg");

  state->connecting = 1;
  struct station_config *cnf = (struct station_config *)param;
  (void)prio;

  /* Best-effort disconnect-reconfig-reconnect. If the device is currently
   * connected, the disconnect will work but the connect will report failure
   * (though it will actually start connecting). If the devices is not
   * connected, the disconnect may fail but the connect will succeed. A
   * solid head-in-the-sand approach seems to be the best tradeoff on
   * functionality-vs-code-size.
   * TODO: maybe use an error callback to at least report if the set config
   * call fails.
   */
  wifi_station_disconnect ();
  wifi_station_set_config (cnf);
  wifi_station_connect ();
  luaM_free(lua_getstate(), cnf);
}

/**
 * Count the number of occurences of a character in a string
 *
 * return the number of times the character was encountered in the string
 */
static int count_char_occurence(const char *input, const char char_to_count) {
  const char *current = input;
  int occur = 0;
    while (*current != 0) {
        if (*current == char_to_count) occur++;
    current++;
  }
  return occur;
}

/* structure used to store the key/value pairs that we find in a HTTP POST body */
struct keypairs_t {
  char **keypairs;
  int keypairs_nb;
};

static void enduser_setup_free_keypairs(struct keypairs_t *kp) {
    if (kp == NULL) return;

    if (kp->keypairs != NULL) {
        for (int i = 0; i < kp->keypairs_nb * 2; i++) {
      free(kp->keypairs[i]);
    }
  }
  free(kp->keypairs);
  free(kp);
}

static struct keypairs_t * enduser_setup_alloc_keypairs(int kp_number ){
    struct keypairs_t *kp = malloc(sizeof(struct keypairs_t));
    os_memset(kp, 0, sizeof(struct keypairs_t));

    kp->keypairs = malloc(kp_number * 2 * sizeof(char *));
    kp->keypairs_nb = kp_number;
    return kp;
}

/**
 * Parses a form-urlencoded body into a struct keypairs_t, which contains an array of key,values strings and the size of the array.
 */
static struct keypairs_t *enduser_setup_get_keypairs_from_form(char *form_body, int form_length) {
  int keypair_nb = count_char_occurence(form_body, '&') + 1;
  int equal_nb = count_char_occurence(form_body, '=');

  if (keypair_nb == 1 && equal_nb == 0) {
    ENDUSER_SETUP_DEBUG("No keypair in form body");
    return NULL;
  }

  struct keypairs_t *kp = enduser_setup_alloc_keypairs(keypair_nb);

  int current_idx = 0;
  int err;

  char *body_copy = malloc(form_length+1);
  os_bzero(body_copy, form_length+1);
  os_memcpy(body_copy, form_body, form_length);
  char *tok = strtok(body_copy, "=");

  char last_tok = '=';
  while (tok) {
    size_t len = strlen(tok);
    kp->keypairs[current_idx] = malloc(len + 1);
    err = enduser_setup_http_urldecode(kp->keypairs[current_idx], tok, len, len + 1);
    if (err) {
      ENDUSER_SETUP_DEBUG("Unable to decode parameter");
      enduser_setup_free_keypairs(kp);
      free(body_copy);
      return NULL;
    }

    current_idx++;
    if (current_idx > keypair_nb*2) {
      ENDUSER_SETUP_DEBUG("Too many keypairs!");
      enduser_setup_free_keypairs(kp);
      free(body_copy);
      return NULL;
    }

    if (last_tok == '=') {
      tok = strtok(NULL, "&"); // now search for the '&'
      last_tok='&';
    } else {
      tok = strtok(NULL, "="); // search for the next '='
      last_tok='=';
    }
  }
  free(body_copy);
  return kp;
}


/**
 * This function saves the form data received when the configuration is sent to the ESP into a eus_params.lua file
 */
static int enduser_setup_write_file_with_extra_configuration_data(char *form_body, int form_length) {
  ENDUSER_SETUP_DEBUG("enduser: write data from posted form");
  ENDUSER_SETUP_DEBUG(form_body);

  // We will save the form data into a file in the LUA format: KEY="VALUE", so that configuration data is available for load in the lua code.
  // As input, we have a string as such: "key1=value1&key2=value2&key3=value%203" (urlencoded), the number of '&' tells us how many keypairs there are (the count + 1)

  struct keypairs_t *kp = enduser_setup_get_keypairs_from_form(form_body, form_length);
  if (kp == NULL || kp->keypairs_nb == 0) {
    ENDUSER_SETUP_DEBUG("enduser: No extra configuration.");
    if (kp != NULL) enduser_setup_free_keypairs(kp);
    return 1;
  }

  // Now that we have the keys and the values, let's save them in a lua file
  int p_file = vfs_open("eus_params.lua", "w");
  if (p_file == 0)
  {
    ENDUSER_SETUP_DEBUG("Can't open file in write mode!");
    enduser_setup_free_keypairs(kp);
    return 1;
  }

  // write all key pairs as KEY="VALUE"\n into a Lua table, example:
  // local p = {}
  // p.wifi_ssid="ssid"
  // p.wifi_password="password"
  // p.device_name="foo-node"
  // return p
  vfs_write(p_file, "local p={}\n", 11);
  int idx = 0;
  for( idx = 0; idx < kp->keypairs_nb*2; idx=idx+2){
    char* to_write = kp->keypairs[idx];
    size_t length = strlen(to_write);

    vfs_write(p_file, "p.", 2);

    vfs_write(p_file, to_write, length);

    vfs_write(p_file, "=\"", 2);

    to_write = kp->keypairs[idx+1];
    length = strlen(to_write);
    vfs_write(p_file, to_write, length);

    vfs_write(p_file, "\"\n", 2);
  }
  vfs_write(p_file, "return p\n", 9);

  vfs_close(p_file);
  enduser_setup_free_keypairs(kp);
  // TODO: we could call back in the LUA with an associative table setup, but this is MVP2...
  return 0;
}

/**
 * Handle HTTP Credentials
 *
 * @return - return 0 if credentials are found and handled successfully
 *           return 1 if credentials aren't found
 *           return 2 if an error occured
 */
static int enduser_setup_http_handle_credentials(char *data, unsigned short data_len)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_http_handle_credentials");

  state->success = 0;
  state->lastStationStatus = 0;


  char *name_str = strstr(data, "wifi_ssid=");
  char *pwd_str = strstr(data, "wifi_password=");

  // in case we dont get a passwd (for open networks)
  if (pwd_str == NULL) {
    pwd_str="wifi_password=";
    ENDUSER_SETUP_DEBUG("No passord provided. Assuming open network");
  }

  if (name_str == NULL)
  {
    ENDUSER_SETUP_DEBUG("SSID string not found");
    return 1;
  }

  int name_field_len = LITLEN("wifi_ssid=");
  int pwd_field_len = LITLEN("wifi_password=");
  char *name_str_start = name_str + name_field_len;
  char *pwd_str_start = pwd_str + pwd_field_len;

  int name_str_len = enduser_setup_get_lenth_of_param_value(name_str_start);
  int pwd_str_len = enduser_setup_get_lenth_of_param_value(pwd_str_start);


  struct station_config *cnf = luaM_malloc(lua_getstate(), sizeof(struct station_config));
  memset(cnf, 0, sizeof(struct station_config));
  cnf->threshold.rssi = -127;
  cnf->threshold.authmode = AUTH_OPEN;

  int err;
  err  = enduser_setup_http_urldecode(cnf->ssid, name_str_start, name_str_len, sizeof(cnf->ssid));
  err |= enduser_setup_http_urldecode(cnf->password, pwd_str_start, pwd_str_len, sizeof(cnf->password));
  if (err != 0 || strlen(cnf->ssid) == 0)
  {
    ENDUSER_SETUP_DEBUG("Unable to decode HTTP parameter to valid password or SSID");
    return 1;
  }

  ENDUSER_SETUP_DEBUG("");
  ENDUSER_SETUP_DEBUG("WiFi Credentials Stored");
  ENDUSER_SETUP_DEBUG("-----------------------");
  ENDUSER_SETUP_DEBUG("name: ");
  ENDUSER_SETUP_DEBUG(cnf->ssid);
  ENDUSER_SETUP_DEBUG("pass: ");
  ENDUSER_SETUP_DEBUG(cnf->password);
  ENDUSER_SETUP_DEBUG("-----------------------");
  ENDUSER_SETUP_DEBUG("");

  task_post_medium(do_station_cfg_handle, (task_param_t) cnf);

  return 0;
}


/**
 * Serve HTML
 *
 * @return - return 0 if html was served successfully
 */
static int enduser_setup_http_serve_header(struct tcp_pcb *http_client, const char *header, uint32_t header_len)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_http_serve_header");

  err_t err = tcp_write (http_client, header, header_len, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK)
  {
    deferred_close (http_client);
    ENDUSER_SETUP_ERROR("http_serve_header failed on tcp_write", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
  }

  return 0;
}


static err_t streamout_sent (void *arg, struct tcp_pcb *pcb, u16_t len)
{
  ENDUSER_SETUP_DEBUG("streamout_sent");

  (void)len;
  unsigned offs = (unsigned)arg;

  if (!state || !state->http_payload_data)
  {
    tcp_abort (pcb);
    return ERR_ABRT;
  }

  unsigned wanted_len = state->http_payload_len - offs;
  unsigned buf_free = tcp_sndbuf (pcb);
  if (buf_free < wanted_len)
    wanted_len = buf_free;

  /* no-copy write */
  err_t err = tcp_write (pcb, state->http_payload_data + offs, wanted_len, 0);
  if (err != ERR_OK)
  {
    ENDUSER_SETUP_DEBUG("streaming out html failed");
    tcp_abort (pcb);
    return ERR_ABRT;
  }

  offs += wanted_len;

  if (offs >= state->http_payload_len)
  {
    tcp_sent (pcb, 0);
    deferred_close (pcb);
    free(state->http_payload_data);
    state->http_payload_data = NULL;
  }
  else
    tcp_arg (pcb, (void *)offs);

  return ERR_OK;
}


/**
 * Serve HTML
 *
 * @return - return 0 if html was served successfully
 */
static int enduser_setup_http_serve_html(struct tcp_pcb *http_client)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_http_serve_html");

  if (state->http_payload_data == NULL)
  {
    enduser_setup_http_load_payload();
  }

  unsigned chunklen = tcp_sndbuf (http_client);
  tcp_arg (http_client, (void *)chunklen);
  tcp_recv (http_client, 0); /* avoid confusion about the tcp_arg */
  tcp_sent (http_client, streamout_sent);
  /* Begin the no-copy stream-out here */
  err_t err = tcp_write (http_client, state->http_payload_data, chunklen, 0);
  if (err != 0)
  {
    ENDUSER_SETUP_ERROR("http_serve_html failed. tcp_write failed", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
  }

  return 0;
}


static void enduser_setup_serve_status(struct tcp_pcb *conn)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_serve_status");

  const char fmt[] =
    "HTTP/1.1 200 OK\r\n"
    "Cache-control:no-cache\r\n"
    "Connection:close\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Content-type:text/plain\r\n"
    "Content-length: %d\r\n"
    "\r\n"
    "%s";
  const char *states[] =
  {
    "Idle.",
    "Connecting to \"%s\".",
    "Failed to connect to \"%s\" - Wrong password.",
    "Failed to connect to \"%s\" - Network not found.",
    "Failed to connect.",
    "Connected to \"%s\" (%s)."
  };

  const size_t num_states = sizeof(states)/sizeof(states[0]);
  uint8_t curr_state = state->lastStationStatus > 0 ? state->lastStationStatus : wifi_station_get_connect_status ();
  if (curr_state < num_states)
  {
    switch (curr_state)
    {
      case STATION_CONNECTING:
      case STATION_WRONG_PASSWORD:
      case STATION_NO_AP_FOUND:
      case STATION_GOT_IP:
      {
        const char *s = states[curr_state];
        struct station_config config;
        wifi_station_get_config(&config);
        config.ssid[31] = '\0';

        struct ip_info ip_info;

        wifi_get_ip_info(STATION_IF , &ip_info);

        char ip_addr[16];
        ip_addr[0] = '\0';
        if (curr_state == STATION_GOT_IP)
        {
          sprintf (ip_addr, "%d.%d.%d.%d", IP2STR(&ip_info.ip.addr));
        }

        int state_len = strlen(s);
        int ip_len = strlen(ip_addr);
        int ssid_len = strlen(config.ssid);
        int status_len = state_len + ssid_len + ip_len + 1;
        char status_buf[status_len];
        memset(status_buf, 0, status_len);
        status_len = sprintf(status_buf, s, config.ssid, ip_addr);

        int buf_len = sizeof(fmt) + status_len + 10; /* 10 = (9+1), 1 byte is '\0' and 9 are reserved for length field */
        char buf[buf_len];
        memset(buf, 0, buf_len);
        int output_len = sprintf(buf, fmt, status_len, status_buf);

        enduser_setup_http_serve_header(conn, buf, output_len);
      }
      break;

      /* Handle non-formatted strings */
      default:
      {
        const char *s = states[curr_state];
        int status_len = strlen(s);
        int buf_len = sizeof(fmt) + status_len + 10; /* 10 = (9+1), 1 byte is '\0' and 9 are reserved for length field */
        char buf[buf_len];
        memset(buf, 0, buf_len);
        int output_len = sprintf(buf, fmt, status_len, s);

        enduser_setup_http_serve_header(conn, buf, output_len);
      }
      break;
    }
  }
  else
  {
    enduser_setup_http_serve_header(conn, http_header_500, LITLEN(http_header_500));
  }
}

static void enduser_setup_serve_status_as_json (struct tcp_pcb *http_client)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_serve_status_as_json");

  /* If the station is currently shut down because of wi-fi channel issue, use the cached status */
  uint8_t curr_status = state->lastStationStatus > 0 ? state->lastStationStatus : wifi_station_get_connect_status ();

  char json_payload[64];

  struct ip_info ip_info;

  if (curr_status == 5)
  {
    wifi_get_ip_info(STATION_IF , &ip_info);
    /* If IP address not yet available, get now */
    if (strlen(ipaddr) == 0)
    {
      sprintf(ipaddr, "%d.%d.%d.%d", IP2STR(&ip_info.ip.addr));
    }
    sprintf(json_payload, "{\"deviceid\":\"%s\", \"status\":%d}", ipaddr, curr_status);
  }
  else
  {
    sprintf(json_payload, "{\"deviceid\":\"%06X\", \"status\":%d}", system_get_chip_id(), curr_status);
  }

  const char fmt[] =
    "HTTP/1.1 200 OK\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: %d\r\n"
    "\r\n"
    "%s";

  int len = strlen(json_payload);
  char buf[strlen(fmt) + NUMLEN(len) + len - 4];
  len = sprintf (buf, fmt, len, json_payload);
  enduser_setup_http_serve_header (http_client, buf, len);
}


static void enduser_setup_handle_OPTIONS (struct tcp_pcb *http_client, char *data, unsigned short data_len)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_handle_OPTIONS");

  const char json[] =
    "HTTP/1.1 200 OK\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 0\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: GET\r\n"
    "Access-Control-Allow-Age: 300\r\n"
    "\r\n";

  const char others[] =
    "HTTP/1.1 200 OK\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

  int type = 0;

  if (strncmp(data, "OPTIONS ", 8) == 0)
  {
    if (strncmp(data + 8, "/aplist", 7) == 0 || strncmp(data + 8, "/setwifi?", 9) == 0 || strncmp(data + 8, "/status.json", 12) == 0)
    {
      enduser_setup_http_serve_header (http_client, json, strlen(json));
      return;
   }
  }
  enduser_setup_http_serve_header (http_client, others, strlen(others));
  return;
}


static void enduser_setup_handle_POST(struct tcp_pcb *http_client, char* data, size_t data_len)
{
    ENDUSER_SETUP_DEBUG("Handling POST");
    if (strncmp(data + 5, "/setwifi ", 9) == 0) // User clicked the submit button
    {
      char* body=strstr(data, "\r\n\r\n");
      if( body == NULL)
      {
        enduser_setup_http_serve_header(http_client, http_header_400, LITLEN(http_header_400));
        return;
      }
      body += 4; // length of the double CRLF found above
      int bodylength = (data + data_len) - body;
      switch (enduser_setup_http_handle_credentials(body, bodylength))
      {
        case 0: {
          // all went fine, extract all the form data into a file
          enduser_setup_write_file_with_extra_configuration_data(body, bodylength);
          // redirect user to the base page with the trying flag
          enduser_setup_http_serve_header(http_client, http_header_302_trying, LITLEN(http_header_302_trying));
          break;
        }
        case 1:
          enduser_setup_http_serve_header(http_client, http_header_400, LITLEN(http_header_400));
          break;
        default:
          ENDUSER_SETUP_ERROR_VOID("http_recvcb failed. Failed to handle wifi credentials.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
          break;
      }
    } else {
      enduser_setup_http_serve_header(http_client, http_header_404, LITLEN(http_header_404));
    }
}


/* --- WiFi AP scanning support -------------------------------------------- */

static void free_scan_listeners (void)
{
  ENDUSER_SETUP_DEBUG("free_scan_listeners");

  if (!state || !state->scan_listeners)
  {
    return;
  }

  scan_listener_t *l = state->scan_listeners , *next = 0;
  while (l)
  {
    next = l->next;
    free (l);
    l = next;
  }
  state->scan_listeners = 0;
}


static void remove_scan_listener (scan_listener_t *l)
{
  ENDUSER_SETUP_DEBUG("remove_scan_listener");

  if (state)
  {
    scan_listener_t **sl = &state->scan_listeners;
    while (*sl)
    {
      /* Remove any and all references to the closed conn from the scan list */
      if (*sl == l)
      {
        *sl = l->next;
        free (l);
        /* No early exit to guard against multi-entry on list */
      }
      else
        sl = &(*sl)->next;
    }
  }
}


static char *escape_ssid (char *dst, const char *src)
{
  for (int i = 0; i < 32 && src[i]; ++i)
  {
    if (src[i] == '\\' || src[i] == '"')
    {
      *dst++ = '\\';
    }
    *dst++ = src[i];
  }
  return dst;
}


static void notify_scan_listeners (const char *payload, size_t sz)
{
  ENDUSER_SETUP_DEBUG("notify_scan_listeners");

  if (!state)
  {
    return;
  }

  for (scan_listener_t *l = state->scan_listeners; l; l = l->next)
  {
    if (tcp_write (l->conn, payload, sz, TCP_WRITE_FLAG_COPY) != ERR_OK)
    {
      ENDUSER_SETUP_DEBUG("failed to send wifi list");
      tcp_abort (l->conn);
    }
    else
      tcp_sent (l->conn, close_once_sent); /* TODO: time-out sends? */
    l->conn = 0;
  }

  free_scan_listeners ();
}


static void on_scan_done (void *arg, STATUS status)
{
  ENDUSER_SETUP_DEBUG("on_scan_done");

  if (!state || !state->scan_listeners)
  {
    return;
  }

  if (status == OK)
  {
    unsigned num_nets = 0;
    for (struct bss_info *wn = arg; wn; wn = wn->next.stqe_next)
    {
      ++num_nets;
    }

    const char header_fmt[] =
      "HTTP/1.1 200 OK\r\n"
      "Connection:close\r\n"
      "Cache-control:no-cache\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Content-type:application/json\r\n"
      "Content-length:%4d\r\n"
      "\r\n";
    const size_t hdr_sz = sizeof (header_fmt) +1 -1; /* +expand %4d, -\0 */

    /* To be able to safely escape a pathological SSID, we need 2*32 bytes */
    const size_t max_entry_sz = 35 + 2*32 + 9; /* {"ssid":"","rssi":,"chan":,"auth":} */
    const size_t alloc_sz = hdr_sz + num_nets * max_entry_sz + 3;
    char *http = calloc (1, alloc_sz);
    if (!http)
    {
      goto serve_500;
    }

    char *p = http + hdr_sz; /* start body where we know it will be */
    /* p[0] will be clobbered when we print the header, so fill it in last */
    ++p;
    for (struct bss_info *wn = arg; wn; wn = wn->next.stqe_next)
    {
      if (wn != arg)
      {
        *p++ = ',';
      }

      const char entry_start[] = "{\"ssid\":\"";
      strcpy (p, entry_start);
      p += sizeof (entry_start) -1;

      p = escape_ssid (p, wn->ssid);

      const char entry_mid[] = "\",\"rssi\":";
      strcpy (p, entry_mid);
      p += sizeof (entry_mid) -1;

      p += sprintf (p, "%d", wn->rssi);

      const char entry_chan[] = ",\"chan\":";
      strcpy (p, entry_chan);
      p += sizeof (entry_chan) -1;

      p += sprintf (p, "%d", wn->channel);

      const char entry_auth[] = ",\"auth\":";
      strcpy (p, entry_auth);
      p += sizeof (entry_auth) -1;

      p += sprintf (p, "%d", wn->authmode);

      *p++ = '}';
    }
    *p++ = ']';

    size_t body_sz = (p - http) - hdr_sz;
    sprintf (http, header_fmt, body_sz);
    http[hdr_sz] = '['; /* Rewrite the \0 with the correct start of body */

    notify_scan_listeners (http, hdr_sz + body_sz);
    ENDUSER_SETUP_DEBUG(http + hdr_sz);

    free (http);
    return;
  }

serve_500:
  notify_scan_listeners (http_header_500, LITLEN(http_header_500));
}

/* ---- end WiFi AP scan support ------------------------------------------- */

static err_t enduser_setup_http_recvcb(void *arg, struct tcp_pcb *http_client, struct pbuf *p, err_t err)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_http_recvcb");

  if (!state || err != ERR_OK)
  {
    if (!state)
      ENDUSER_SETUP_DEBUG("ignoring received data while stopped");
    tcp_abort (http_client);
    return ERR_ABRT;
  }

  tcp_arg_t *tcp_arg_ptr = arg;

  if (!p) /* remote side closed, close our end too */
  {
    ENDUSER_SETUP_DEBUG("connection closed");
    if (tcp_arg_ptr) {
      if (tcp_arg_ptr->struct_type == SCAN_LISTENER_STRUCT_TYPE) {
        remove_scan_listener ((scan_listener_t *)tcp_arg_ptr);
      } else if (tcp_arg_ptr->struct_type == HTTP_REQUEST_BUFFER_STRUCT_TYPE) {
        free(tcp_arg_ptr);
      }
    }

    deferred_close (http_client);
    return ERR_OK;
  }

  http_request_buffer_t *hrb;
  if (!tcp_arg_ptr) {
    hrb = calloc(1, sizeof(*hrb));
    if (!hrb) {
      goto general_fail;
    }
    hrb->struct_type = HTTP_REQUEST_BUFFER_STRUCT_TYPE;
    tcp_arg(http_client, hrb);
  } else if (tcp_arg_ptr->struct_type == HTTP_REQUEST_BUFFER_STRUCT_TYPE) {
    hrb = (http_request_buffer_t *) tcp_arg_ptr;
  } else {
    goto general_fail;
  }

  // Append the new data
  size_t newlen = hrb->length + p->tot_len;
  void *old_hrb = hrb;
  hrb = realloc(hrb, sizeof(*hrb) + newlen + 1);
  tcp_arg(http_client, hrb);
  if (!hrb) {
    free(old_hrb);
    goto general_fail;
  }

  pbuf_copy_partial(p, hrb->data + hrb->length, p->tot_len, 0);
  hrb->data[newlen] = 0;
  hrb->length = newlen;

  tcp_recved (http_client, p->tot_len);
  pbuf_free (p);

  // see if we have the whole request.
  // Rely on the fact that the header should not contain a null character
  char *end_of_header = strstr(hrb->data, "\r\n\r\n");
  if (end_of_header == 0) {
    return ERR_OK;
  }

  end_of_header += 4;

  // We have the entire header, now see if there is any content. If we don't find the
  // content-length header, then there is no content and we can process immediately.
  // The content-length header can also be missing if the browser is using chunked
  // encoding.

  bool is_chunked = FALSE;
  for (const char *hdr = hrb->data; hdr && hdr < end_of_header; hdr = strchr(hdr, '\n')) {
    hdr += 1; // Skip the \n
    if (strncasecmp(hdr, "transfer-encoding:", 18) == 0) {
      const char *field = hdr + 18;

      while (*field != '\n') {
        if (memcmp(field, "chunked", 7) == 0) {
          is_chunked = TRUE;
          break;
        }
        field++;
      }
    }
    if (strncasecmp(hdr, "Content-length:", 15) == 0) {
      // There is a content-length header
      const char *field = hdr + 15;
      size_t extra = strtol(field + 1, 0, 10);
      if (extra + (end_of_header - hrb->data) > hrb->length) {
        return ERR_OK;
      }
    }
  }

  if (is_chunked) {
    // More complex to determine if the whole body has arrived
    // Format is one or more chunks each preceded by their length (in hex)
    // A zero length chunk ends the body
    const char *ptr = end_of_header;
    bool seen_end = FALSE;

    while (ptr < hrb->data + hrb->length && ptr > hrb->data) {
      size_t chunk_len = strtol(ptr, 0, 16);
      // Skip to end of chunk length (note that there can be parameters after the length)
      ptr = strchr(ptr, '\n');
      if (!ptr) {
        // Don't have the entire chunk header
        return ERR_OK;
      }
      ptr++;
      ptr += chunk_len;
      if (chunk_len == 0) {
        seen_end = TRUE;
        break;
      }
      if (ptr + 2 > hrb->data + hrb->length) {
        // We don't have the CRLF yet
        return ERR_OK;
      }
      if (memcmp(ptr, "\r\n", 2)) {
        // Bail out here -- something bad happened
        goto general_fail;
      }
      ptr += 2;
    }
    if (!seen_end) {
      // Still waiting for the end chunk
      return ERR_OK;
    }

    // Now rewrite the buffer to eliminate all the chunk headers
    const char *src = end_of_header;
    char *dst = end_of_header;

    while (src < hrb->data + hrb->length && src > hrb->data) {
      size_t chunk_len = strtol(src, 0, 16);
      src = strchr(src, '\n');
      src++;
      if (chunk_len == 0) {
        break;
      }
      memmove(dst, src, chunk_len);
      dst += chunk_len;
      src += chunk_len + 2;
    }
    *dst = '\0';    // Move the null termination down
    hrb->length = dst - hrb->data;  // Adjust the length down
  }

  err_t ret = ERR_OK;

  char *data = hrb->data;
  size_t data_len = hrb->length;

  tcp_arg(http_client, 0);    // Forget the data pointer.

#if ENDUSER_SETUP_DEBUG_SHOW_HTTP_REQUEST
  ENDUSER_SETUP_DEBUG(data);
#endif

  if (strncmp(data, "GET ", 4) == 0)
  {
    if (strncmp(data + 4, "/ ", 2) == 0 || strncmp(data + 4, "/?", 2) == 0)
    {
      if (enduser_setup_http_serve_html(http_client) != 0)
      {
        goto general_fail;
      }
      else
      {
        goto free_out; /* streaming now in progress */
      }
    }
    else if (strncmp(data + 4, "/aplist", 7) == 0)
    {
      /* Don't do an AP Scan while station is trying to connect to Wi-Fi */
      if (state->connecting == 0)
      {
        scan_listener_t *sl = malloc (sizeof (scan_listener_t));
        if (!sl)
        {
          ENDUSER_SETUP_ERROR("out of memory", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_NONFATAL);
        }

        sl->struct_type = SCAN_LISTENER_STRUCT_TYPE;

        bool already = (state->scan_listeners != NULL);

        tcp_arg (http_client, sl);
        /* TODO: check if also need a tcp_err() cb, or if recv() is enough */
        sl->conn = http_client;
        sl->next = state->scan_listeners;
        state->scan_listeners = sl;

        if (!already)
        {
          if (!wifi_station_scan(NULL, on_scan_done))
          {
            enduser_setup_http_serve_header(http_client, http_header_500, LITLEN(http_header_500));
            deferred_close (sl->conn);
            sl->conn = 0;
            free_scan_listeners();
          }
        }
        goto free_out; /* request queued */
      }
      else
      {
        /* Return No Content status to the caller */
        enduser_setup_http_serve_header(http_client, http_header_204, LITLEN(http_header_204));
      }
    }
    else if (strncmp(data + 4, "/status.json", 12) == 0)
    {
      enduser_setup_serve_status_as_json(http_client);
    }
    else if (strncmp(data + 4, "/status", 7) == 0)
    {
      enduser_setup_serve_status(http_client);
    }
    else if (strncmp(data + 4, "/update?", 8) == 0)
    {
      switch (enduser_setup_http_handle_credentials(data, data_len))
      {
        case 0:
          enduser_setup_http_serve_header(http_client, http_header_302, LITLEN(http_header_302));
          break;
        case 1:
          enduser_setup_http_serve_header(http_client, http_header_400, LITLEN(http_header_400));
          break;
        default:
          ENDUSER_SETUP_ERROR("http_recvcb failed. Failed to handle wifi credentials.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
          break;
      }
    }
    else
    {
      // All other URLs redirect to http://nodemcu.portal/ -- this triggers captive portal.
      enduser_setup_http_serve_header(http_client, http_header_302, LITLEN(http_header_302));
    }
  }
  else if (strncmp(data, "OPTIONS ", 8) == 0)
  {
     enduser_setup_handle_OPTIONS(http_client, data, data_len);
  }
  else if (strncmp(data, "POST ", 5) == 0)
  {
    enduser_setup_handle_POST(http_client, data, data_len);
  }
  else /* not GET or OPTIONS */
  {
    enduser_setup_http_serve_header(http_client, http_header_405, LITLEN(http_header_405));
  }

  deferred_close (http_client);

free_out:
  free (hrb);
  return ret;

general_fail:
  ENDUSER_SETUP_ERROR("http_recvcb failed. Unable to send HTML.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
}


static err_t enduser_setup_http_connectcb(void *arg, struct tcp_pcb *pcb, err_t err)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_http_connectcb");

  if (!state)
  {
    ENDUSER_SETUP_DEBUG("connect callback but no state?!");
    tcp_abort (pcb);
    return ERR_ABRT;
  }

  tcp_accepted (state->http_pcb);
  tcp_arg(pcb, 0);              // Initialize to known value
  tcp_recv (pcb, enduser_setup_http_recvcb);
  tcp_nagle_disable (pcb);
  return ERR_OK;
}


static int enduser_setup_http_start(void)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_http_start");
  struct tcp_pcb *pcb = tcp_new ();
  if (pcb == NULL)
  {
    ENDUSER_SETUP_ERROR("http_start failed. Memory allocation failed (http_pcb == NULL).", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }

  if (tcp_bind (pcb, IP_ADDR_ANY, 80) != ERR_OK)
  {
    ENDUSER_SETUP_ERROR("http_start bind failed", ENDUSER_SETUP_ERR_SOCKET_ALREADY_OPEN, ENDUSER_SETUP_ERR_FATAL);
  }

  state->http_pcb = tcp_listen (pcb);
  if (!state->http_pcb)
  {
    tcp_abort(pcb); /* original wasn't freed for us */
    ENDUSER_SETUP_ERROR("http_start listen failed", ENDUSER_SETUP_ERR_SOCKET_ALREADY_OPEN, ENDUSER_SETUP_ERR_FATAL);
  }

  tcp_accept (state->http_pcb, enduser_setup_http_connectcb);

  /* TODO: check lwip tcp timeouts */
#if 0
  err = espconn_regist_time(state->espconn_http_tcp, 2, 0);
  if (err == ESPCONN_ARG)
  {
    ENDUSER_SETUP_ERROR("http_start failed. Unable to set TCP timeout.", ENDUSER_SETUP_ERR_CONNECTION_NOT_FOUND, ENDUSER_SETUP_ERR_NONFATAL | ENDUSER_SETUP_ERR_NO_RETURN);
  }
#endif

  int err = enduser_setup_http_load_payload();
  if (err == 1)
  {
    ENDUSER_SETUP_DEBUG("enduser_setup_http_start info. Loaded default HTML.");
  }
  else if (err == 2)
  {
    ENDUSER_SETUP_ERROR("http_start failed. Unable to allocate memory for HTTP payload.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }

  return 0;
}


static void enduser_setup_http_stop(void)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_http_stop");

  if (state && state->http_pcb)
  {
    tcp_close (state->http_pcb); /* cannot fail for listening sockets */
    state->http_pcb = 0;
  }
}

static void enduser_setup_ap_stop(void)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_ap_stop");

  wifi_set_opmode(~SOFTAP_MODE & wifi_get_opmode());
}


static void enduser_setup_ap_start(void)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_ap_start");

  struct softap_config cnf;
  memset(&(cnf), 0, sizeof(struct softap_config));

#ifndef ENDUSER_SETUP_AP_SSID
  #define ENDUSER_SETUP_AP_SSID "NodeMCU"
#endif

  if (state->ap_ssid) {
    strncpy(cnf.ssid, state->ap_ssid, sizeof(cnf.ssid));
    cnf.ssid_len = strlen(cnf.ssid);
  } else {
    char ssid[] = ENDUSER_SETUP_AP_SSID;
    int ssid_name_len = strlen(ssid);
    memcpy(&(cnf.ssid), ssid, ssid_name_len);

    uint8_t mac[6];
    wifi_get_macaddr(SOFTAP_IF, mac);
    cnf.ssid[ssid_name_len] = '_';
    sprintf(cnf.ssid + ssid_name_len + 1, "%02X%02X%02X", mac[3], mac[4], mac[5]);
    cnf.ssid_len = ssid_name_len + 7;
  }
  cnf.channel = state == NULL? 1 : state->softAPchannel;
  cnf.authmode = AUTH_OPEN;
  cnf.ssid_hidden = 0;
  cnf.max_connection = 5;
  cnf.beacon_interval = 100;
  wifi_set_opmode(STATIONAP_MODE);
  wifi_softap_set_config(&cnf);

#if ENDUSER_SETUP_DEBUG_ENABLE
  char debuginfo[100];
  sprintf(debuginfo, "SSID: %s, CHAN: %d", cnf.ssid, cnf.channel);
  ENDUSER_SETUP_DEBUG(debuginfo);
#endif
}

static void on_initial_scan_done (void *arg, STATUS status)
{
  ENDUSER_SETUP_DEBUG("on_initial_scan_done");

  if (state == NULL)
  {
    return;
  }

  int8_t rssi = -100;

  if (status == OK)
  {
    /* Find the strongest signal and use the same wi-fi channel for the SoftAP. This is based on an assumption that end-user
     * will likely be choosing that AP to connect to. Since ESP only has one radio, STA and AP must share same channel. This
     * algorithm tries to minimize the SoftAP unavailability when the STA is connecting to verify. If the STA must switch to
     * another wi-fi channel, then the SoftAP will follow it, but the end-user's device will not know that the SoftAP is no
     * longer there until it times out. To mitigate, we try to prevent the need to switch channels, and if a switch does occur,
     * be quick about returning to this channel so that status info can be delivered to the end-user's device before shutting
     * down EUS.
     */
    for (struct bss_info *wn = arg; wn; wn = wn->next.stqe_next)
    {
      if (wn->rssi > rssi)
      {
        state->softAPchannel = wn->channel;
        rssi = wn->rssi;
      }
    }
  }

  enduser_setup_ap_start();
  enduser_setup_check_station_start();
}

static void enduser_setup_dns_recv_callback(void *arg, char *recv_data, unsigned short recv_len)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_dns_recv_callback.");

  struct espconn *callback_espconn = arg;
  struct ip_info ip_info;

  uint32_t qname_len = strlen(&(recv_data[12])) + 1; /* \0=1byte */
  uint32_t dns_reply_static_len = (uint32_t) sizeof(dns_header) + (uint32_t) sizeof(dns_body) + 2 + 4; /* dns_id=2bytes, ip=4bytes */
  uint32_t dns_reply_len = dns_reply_static_len + qname_len;

#if ENDUSER_SETUP_DEBUG_ENABLE
  char *qname = malloc(qname_len + 12);
  if (qname != NULL)
  {
    sprintf(qname, "DNS QUERY = %s", &(recv_data[12]));

    uint32_t p;
    int i, j;

    for(i=12;i<(int)strlen(qname);i++)
    {
      p=qname[i];
      for(j=0;j<(int)p;j++)
      {
        qname[i]=qname[i+1];
        i=i+1;
      }
      qname[i]='.';
    }
    qname[i-1]='\0';
    ENDUSER_SETUP_DEBUG(qname);
    free(qname);
  }
#endif

  uint8_t if_mode = wifi_get_opmode();
  if ((if_mode & SOFTAP_MODE) == 0)
  {
    ENDUSER_SETUP_ERROR_VOID("dns_recv_callback failed. Interface mode not supported.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }

  uint8_t if_index = (if_mode == STATION_MODE? STATION_IF : SOFTAP_IF);
  if (wifi_get_ip_info(if_index , &ip_info) == false)
  {
    ENDUSER_SETUP_ERROR_VOID("dns_recv_callback failed. Unable to get interface IP.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }


  char *dns_reply = (char *) malloc(dns_reply_len);
  if (dns_reply == NULL)
  {
    ENDUSER_SETUP_ERROR_VOID("dns_recv_callback failed. Failed to allocate memory.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_NONFATAL);
  }

  uint32_t insert_byte = 0;
  memcpy(&(dns_reply[insert_byte]), recv_data, 2);
  insert_byte += 2;
  memcpy(&(dns_reply[insert_byte]), dns_header, sizeof(dns_header));
  insert_byte += (uint32_t) sizeof(dns_header);
  memcpy(&(dns_reply[insert_byte]), &(recv_data[12]), qname_len);
  insert_byte += qname_len;
  memcpy(&(dns_reply[insert_byte]), dns_body, sizeof(dns_body));
  insert_byte += (uint32_t) sizeof(dns_body);
  memcpy(&(dns_reply[insert_byte]), &(ip_info.ip), 4);

  /* SDK 1.4.0 changed behaviour, for UDP server need to look up remote ip/port */
  remot_info *pr = 0;
  if (espconn_get_connection_info(callback_espconn, &pr, 0) != ESPCONN_OK)
  {
    ENDUSER_SETUP_ERROR_VOID("dns_recv_callback failed. Unable to get IP of UDP sender.", ENDUSER_SETUP_ERR_CONNECTION_NOT_FOUND, ENDUSER_SETUP_ERR_FATAL);
  }
  callback_espconn->proto.udp->remote_port = pr->remote_port;
  os_memmove(callback_espconn->proto.udp->remote_ip, pr->remote_ip, 4);

  int8_t err;
  err = espconn_send(callback_espconn, dns_reply, dns_reply_len);
  free(dns_reply);
  if (err == ESPCONN_MEM)
  {
    ENDUSER_SETUP_ERROR_VOID("dns_recv_callback failed. Failed to allocate memory for send.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }
  else if (err == ESPCONN_ARG)
  {
    ENDUSER_SETUP_ERROR_VOID("dns_recv_callback failed. Can't execute transmission.", ENDUSER_SETUP_ERR_CONNECTION_NOT_FOUND, ENDUSER_SETUP_ERR_FATAL);
  }
  else if (err == ESPCONN_MAXNUM)
  {
    ENDUSER_SETUP_ERROR_VOID("dns_recv_callback failed. Buffer full. Discarding...", ENDUSER_SETUP_ERR_MAX_NUMBER, ENDUSER_SETUP_ERR_NONFATAL);
  }
  else if (err == ESPCONN_IF)
  {
    ENDUSER_SETUP_ERROR_VOID("dns_recv_callback failed. Send UDP data failed", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
  }
  else if (err != 0)
  {
    ENDUSER_SETUP_ERROR_VOID("dns_recv_callback failed. espconn_send failed", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }
}


static void enduser_setup_free(void)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_free");

  if (state == NULL)
  {
    return;
  }

  /* Make sure no running timers are left. */
  os_timer_disarm(&(state->check_station_timer));
  os_timer_disarm(&(state->shutdown_timer));

  if (state->espconn_dns_udp != NULL)
  {
    if (state->espconn_dns_udp->proto.udp != NULL)
    {
      free(state->espconn_dns_udp->proto.udp);
    }
    free(state->espconn_dns_udp);
  }

  free(state->http_payload_data);

  free_scan_listeners ();

  free(state->ap_ssid);

  free(state);
  state = NULL;
}


static int enduser_setup_dns_start(void)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_dns_start");

  if (state->espconn_dns_udp != NULL)
  {
    ENDUSER_SETUP_ERROR("dns_start failed. Appears to already be started (espconn_dns_udp != NULL).", ENDUSER_SETUP_ERR_ALREADY_INITIALIZED, ENDUSER_SETUP_ERR_FATAL);
  }
  state->espconn_dns_udp = (struct espconn *) malloc(sizeof(struct espconn));
  if (state->espconn_dns_udp == NULL)
  {
    ENDUSER_SETUP_ERROR("dns_start failed. Memory allocation failed (espconn_dns_udp == NULL).", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }

  esp_udp *esp_udp_data = (esp_udp *) malloc(sizeof(esp_udp));
  if (esp_udp_data == NULL)
  {
    ENDUSER_SETUP_ERROR("dns_start failed. Memory allocation failed (esp_udp == NULL).", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }

  memset(state->espconn_dns_udp, 0, sizeof(struct espconn));
  memset(esp_udp_data, 0, sizeof(esp_udp));
  state->espconn_dns_udp->proto.udp = esp_udp_data;
  state->espconn_dns_udp->type = ESPCONN_UDP;
  state->espconn_dns_udp->state = ESPCONN_NONE;
  esp_udp_data->local_port = 53;

  int8_t err;
  err = espconn_regist_recvcb(state->espconn_dns_udp, enduser_setup_dns_recv_callback);
  if (err != 0)
  {
    ENDUSER_SETUP_ERROR("dns_start failed. Couldn't add receive callback, unknown error.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }

  err = espconn_create(state->espconn_dns_udp);
  if (err == ESPCONN_ISCONN)
  {
    ENDUSER_SETUP_ERROR("dns_start failed. Couldn't create connection, already listening for that connection.", ENDUSER_SETUP_ERR_SOCKET_ALREADY_OPEN, ENDUSER_SETUP_ERR_FATAL);
  }
  else if (err == ESPCONN_MEM)
  {
    ENDUSER_SETUP_ERROR("dns_start failed. Couldn't create connection, out of memory.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }
  else if (err != 0)
  {
    ENDUSER_SETUP_ERROR("dns_start failed. Couldn't create connection, unknown error.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }

  return 0;
}


static void enduser_setup_dns_stop(void)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_dns_stop");

  if (state != NULL && state->espconn_dns_udp != NULL)
  {
    espconn_delete(state->espconn_dns_udp);
  }
}


static int enduser_setup_init(lua_State *L)
{
  /* Note: Normal to not see this debug message on first invocation because debug callback is set below */
  ENDUSER_SETUP_DEBUG("enduser_setup_init");

  /* Defer errors until the bottom, so that callbacks can be set, if applicable, to handle debug and error messages */
  int ret = 0;

  if (state != NULL)
  {
    ret = ENDUSER_SETUP_ERR_ALREADY_INITIALIZED;
  }
  else
  {
    state = (enduser_setup_state_t *) calloc(1, sizeof(enduser_setup_state_t));

    if (state == NULL)
    {
      ret = ENDUSER_SETUP_ERR_OUT_OF_MEMORY;
    }
    else
    {
      memset(state, 0, sizeof(enduser_setup_state_t));

      state->lua_connected_cb_ref = LUA_NOREF;
      state->lua_err_cb_ref = LUA_NOREF;
      state->lua_dbg_cb_ref = LUA_NOREF;

      state->softAPchannel = 1;
      state->success = 0;
      state->callbackDone = 0;
      state->lastStationStatus = 0;
      state->connecting = 0;
    }
  }

  int argno = 1;

  if (lua_isstring(L, argno)) {
    /* Get the SSID */
    state->ap_ssid = strdup(lua_tostring(L, argno));
    argno++;
  }

  if (!lua_isnoneornil(L, argno))
  {
    lua_pushvalue(L, argno);
    state->lua_connected_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  argno++;

  if (!lua_isnoneornil(L, argno))
  {
    lua_pushvalue (L, argno);
    state->lua_err_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  argno++;

  if (!lua_isnoneornil(L, argno))
  {
    lua_pushvalue (L, argno);
    state->lua_dbg_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    ENDUSER_SETUP_DEBUG("enduser_setup_init: Debug callback has been set");
  }

  if (ret == ENDUSER_SETUP_ERR_ALREADY_INITIALIZED)
  {
    ENDUSER_SETUP_ERROR("init failed. Appears to already be started. EUS will shut down now.", ENDUSER_SETUP_ERR_ALREADY_INITIALIZED, ENDUSER_SETUP_ERR_FATAL);
  }
  else if (ret == ENDUSER_SETUP_ERR_OUT_OF_MEMORY)
  {
    ENDUSER_SETUP_ERROR("init failed. Unable to allocate memory.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }

  return ret;
}


static int enduser_setup_manual(lua_State *L)
{
  if (!lua_isnoneornil (L, 1))
  {
    manual = lua_toboolean (L, 1);
  }
  lua_pushboolean (L, manual);
  return 1;
}


static int enduser_setup_start(lua_State *L)
{
  /* Note: The debug callback is set in enduser_setup_init. It's normal to not see this debug message on first invocation. */
  ENDUSER_SETUP_DEBUG("enduser_setup_start");

  ipaddr[0] = '\0';

  if (!do_station_cfg_handle)
  {
    do_station_cfg_handle = task_get_id(do_station_cfg);
  }

  if(enduser_setup_init(L))
  {
    goto failed;
  }

  if (!manual)
  {
    ENDUSER_SETUP_DEBUG("Performing AP Scan to identify likely AP's channel. Enabling Station if it wasn't already.");
    wifi_set_opmode(STATION_MODE | wifi_get_opmode());
    wifi_station_scan(NULL, on_initial_scan_done);
  }
  else
  {
   enduser_setup_check_station_start();
  }

  if(enduser_setup_dns_start())
  {
    goto failed;
  }

  if(enduser_setup_http_start())
  {
    goto failed;
  }

  goto out;

failed:
  enduser_setup_stop(lua_getstate());
out:
  return 0;
}


/**
 * A wrapper needed for type-reasons strictness reasons.
 */
static void enduser_setup_stop_callback(void *ptr)
{
  enduser_setup_stop(lua_getstate());
}


static int enduser_setup_stop(lua_State* L)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_stop");

  if (!manual)
  {
    enduser_setup_ap_stop();
  }
  if (state != NULL && state->success && !state->callbackDone)
  {
    wifi_set_opmode(STATION_MODE | wifi_get_opmode());
    wifi_station_connect();
    enduser_setup_connected_callback();
  }
  enduser_setup_dns_stop();
  enduser_setup_http_stop();
  enduser_setup_free();

  return 0;
}


LROT_BEGIN(enduser_setup, NULL, 0)
  LROT_FUNCENTRY( manual, enduser_setup_manual )
  LROT_FUNCENTRY( start, enduser_setup_start )
  LROT_FUNCENTRY( stop, enduser_setup_stop )
LROT_END(enduser_setup, NULL, 0)


NODEMCU_MODULE(ENDUSER_SETUP, "enduser_setup", enduser_setup, NULL);
