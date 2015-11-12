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
 */


#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "user_interface.h"
#include "espconn.h"
#include "flash_fs.h"


#define MIN(x, y)  (((x) < (y)) ? (x) : (y))

#define ENDUSER_SETUP_ERR_FATAL        (1 << 0)
#define ENDUSER_SETUP_ERR_NONFATAL     (1 << 1)
#define ENDUSER_SETUP_ERR_NO_RETURN    (1 << 2)

#define ENDUSER_SETUP_ERR_OUT_OF_MEMORY          1
#define ENDUSER_SETUP_ERR_CONNECTION_NOT_FOUND   2
#define ENDUSER_SETUP_ERR_UNKOWN_ERROR           3
#define ENDUSER_SETUP_ERR_SOCKET_ALREADY_OPEN    4


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

static const char http_html_filename[] = "index.html";
static const char http_header_200[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
static const char http_header_404[] = "HTTP/1.1 404 Not Found\r\n";
static const char http_html_backup[] = "<!DOCTYPE html><html><head><meta charset=utf-8><meta name=viewport content='width=380'><title>Connect gadget to you WiFi</title><style media=screen type=text/css>*{margin:0;padding:0}html{height:100%;background:linear-gradient(rgba(196,102,0,.2),rgba(155,89,182,.2)),url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEYAAAA8AgMAAACm+SSwAAAADFBMVEVBR1FFS1VHTlg8Q0zU/YXIAAADVElEQVQ4yy1TTYvTUBQ9GTKiYNoodsCF4MK6U4TZChOhiguFWHyBFzqlLl4hoeNvEBeCrlrhBVKq1EUKLTP+hvi1GyguXqBdiZCBzGqg20K8L3hDQnK55+OeJNguHx6UujYl3dL5ALn4JOIUluAqeAWciyGaSdvngOWzNT+G0UyGUOxVOAdqkjXDCbBiUyjZ5QzYEbGadYAi6kHxth+kthXNVNCDofwhGv1D4QGGiM9iAjbCHgr2iUUpDJbs+VPQ4xAr2fX7KXbkOJMdok965Ksb+6lrjdkem8AshIuHm9Nyu19uTunYlOXDTQqi8VgeH0kBXH2xq/ouiMZPzuMukymutrBmulUTovC6HqNFW2ZOiqlpSXZOTvSUeUPxChjxol8BLbRy4gJuhV7OR4LRVBs3WQ9VVAU7SXgK2HeUrOj7bC8YsUgr3lEV/TXB7hK90EBnxaeg1Ov15bY80M736ekCGesGAaGvG0Ct4WRkVQVHIgIM9xJgvSFfPay8Q6GNv7VpR7xUnkvhnMQCJDYkYOtNLihV70tCU1Sk+BQrpoP+HLHUrJkuta40C6LP5GvBv+Hqo10ATxxFrTPvNdPr7XwgQud6RvQN/sXjBGzqbU27wcj9cgsyvSTrpyXV8gKpXeNJU3aFl7MOdldzV4+HfO19jBa5f2IjWwx1OLHIvFHkqbBj20ro1g7nDfY1DpScvDRUNARgjMMVO0zoMjKxJ6uWCPP+YRAWbGoaN8kXYHmLjB9FXLGOazfFVCvOgqzfnicNPrHtPKlex2ye824gMza0cTZ2sS2Xm7Qst/UfFw8O6vVtmUKxZy9xFgzMys5cJ5fxZw4y37Ufk1Dsfb8MqOjYxE3ZMWxiDcO0PYUaD2ys+8OW1pbB7/e3sfZeGVCL0Q2aMjjPdm2sxADuejZxHJAd8dO9DSUdA0V8/NggRRanDkBrANn8yHlEQOn/MmwoQfQF7xgmKDnv520bS/pgylP67vf3y2V5sCwfoCEMkZClgOfJAFX9eXefR2RpnmRs4CDVPceaRfoFzCkJVJX27vWZnoqyvmtXU3+dW1EIXIu8Qg5Qta4Zlv7drUCoWe8/8MXzaEwux7ESE9h6qnHj3mIO0/D9RvzfxPmjWiQ1vbeSk4rrHwhAre35EEVaAAAAAElFTkSuQmCC)}body{font-family:arial,verdana}div{position:absolute;margin:auto;top:0;right:0;bottom:0;left:0;width:320px;height:274px}form{width:320px;text-align:center;position:relative}form fieldset{background:#fff;border:0 none;border-radius:5px;box-shadow:0 0 15px 1px rgba(0,0,0,.4);padding:20px 30px;box-sizing:border-box}form input{padding:15px;border:1px solid #ccc;border-radius:3px;margin-bottom:10px;width:100%;box-sizing:border-box;font-family:montserrat;color:#2C3E50;font-size:13px}form .action-button{width:100px;background:#27AE60;font-weight:700;color:#fff;border:0 none;border-radius:3px;cursor:pointer;padding:10px 5px;margin:10px 5px}#msform .action-button:focus,form .action-button:hover{box-shadow:0 0 0 2px #fff,0 0 0 3px #27AE60}.fs-title{font-size:15px;text-transform:uppercase;color:#2C3E50;margin-bottom:10px}.fs-subtitle{font-weight:400;font-size:13px;color:#666;margin-bottom:20px}</style><body><div><form method='post'><fieldset><h2 class=fs-title>WiFi Login</h2><h3 class=fs-subtitle>Connect gadget to your WiFi</h3><input autocorrect=off autocapitalize=none name=wifi_ssid placeholder='WiFi Name'> <input type=password name=wifi_password placeholder='Password'1> <input type=submit name=save class='submit action-button' value='Save'></fieldset></form></div>";


typedef struct
{
  lua_State *lua_L;
  struct espconn *espconn_dns_udp;
  struct espconn *espconn_http_tcp;
  char *http_payload_data;
  uint32_t http_payload_len;
  os_timer_t check_station_timer;
  int lua_connected_cb_ref;
  int lua_err_cb_ref;
  int lua_dbg_cb_ref;
} enduser_setup_state_t;

static enduser_setup_state_t *state;

static int enduser_setup_start(lua_State* L);
static int enduser_setup_stop(lua_State* L);
static void enduser_setup_station_start(void);
static void enduser_setup_station_start(void);
static void enduser_setup_ap_start(void);
static void enduser_setup_ap_stop(void);
static void enduser_setup_check_station(void);
static void enduser_setup_debug(lua_State *L, const char *str);


#define ENDUSER_SETUP_DEBUG_ENABLE 0
#if ENDUSER_SETUP_DEBUG_ENABLE
#define ENDUSER_SETUP_DEBUG(l, str) enduser_setup_debug(l, str)
#else
#define ENDUSER_SETUP_DEBUG(l, str)
#endif


static void enduser_setup_debug(lua_State *L, const char *str)
{
  if(state != NULL && L != NULL && state->lua_dbg_cb_ref != LUA_NOREF)
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, state->lua_dbg_cb_ref);
    lua_pushstring(L, str);
    lua_call(L, 1, 0);
  }
}


#define ENDUSER_SETUP_ERROR(str, err, err_severity) if (err_severity & ENDUSER_SETUP_ERR_FATAL) enduser_setup_stop(state->lua_L);\
                                                    enduser_setup_error(state->lua_L, str, err);\
                                                    if (!(err_severity & ENDUSER_SETUP_ERR_NO_RETURN)) return err

#define ENDUSER_SETUP_ERROR_VOID(str, err, err_severity) if (err_severity & ENDUSER_SETUP_ERR_FATAL) enduser_setup_stop(state->lua_L);\
                                                         enduser_setup_error(state->lua_L, str, err);\
                                                         if (!(err_severity & ENDUSER_SETUP_ERR_NO_RETURN)) return


static void enduser_setup_error(lua_State *L, const char *str, int err)
{
  ENDUSER_SETUP_DEBUG(L, "enduser_setup_error");

  if (state != NULL && L != NULL && state->lua_err_cb_ref != LUA_NOREF)
  {
    lua_rawgeti (L, LUA_REGISTRYINDEX, state->lua_err_cb_ref);
    lua_pushnumber(L, err);
    lua_pushstring(L, str);
    lua_call (L, 2, 0);
  }
}


static void enduser_setup_connected_callback(lua_State *L)
{
  ENDUSER_SETUP_DEBUG(L, "enduser_setup_connected_callback");

  if(state != NULL && L != NULL && state->lua_connected_cb_ref != LUA_NOREF)
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, state->lua_connected_cb_ref);
    lua_call(L, 0, 0);
  }
}


static void enduser_setup_check_station_start(void)
{

  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_check_station_start");

  os_timer_setfn(&(state->check_station_timer), enduser_setup_check_station, NULL);
  os_timer_arm(&(state->check_station_timer), 1*1000, TRUE);
}


static void enduser_setup_check_station_stop(void)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_check_station_stop");

  os_timer_disarm(&(state->check_station_timer));
}


/**
 * Check Station
 *
 * Check that we've successfully entered station mode.
 */
static void enduser_setup_check_station(void)
{
  struct ip_info ip;
  c_memset(&ip, 0, sizeof(struct ip_info));

  wifi_get_ip_info(STATION_IF, &ip);

  int i;
  char has_ip = 0;
  for (i = 0; i < sizeof(struct ip_info); ++i)
  {
    has_ip |= ((char *) &ip)[i];
  }

  if (has_ip == 0)
  {
    return;
  }

  struct station_config cnf;
  wifi_station_get_config(&cnf);

  enduser_setup_connected_callback(state->lua_L);
  enduser_setup_stop(NULL);
}


/**
 * Search String
 *
 * Search string for first occurance of any char in srch_str.
 *
 * @return -1 iff no occurance of char was found.
 */
static int enduser_setup_srch_str(const char *str, const char *srch_str)
{
  int srch_str_len = c_strlen(srch_str);
  int first_hit = INT_MAX;

  int i;
  for (i = 0; i < srch_str_len; ++i)
  {
    char *char_ptr = strchr(str, srch_str[i]);
    if (char_ptr == NULL)
    {
      continue;
    }
    int char_idx = char_ptr - str;
    first_hit = MIN(first_hit, char_idx);
  }
  if (first_hit == INT_MAX)
  {
    return -1;
  }
  return first_hit;
}


/**
 * Load HTTP Payload
 *
 * @return - 0 iff payload loaded successfully
 *           1 iff backup html was loaded
 *           2 iff out of memory
 */
static int enduser_setup_http_load_payload(void)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_http_load_payload");

  int f = fs_open(http_html_filename, fs_mode2flag("r"));
  int err = fs_seek(f, 0, FS_SEEK_END);
  int file_len = (int) fs_tell(f);
  int err2 = fs_seek(f, 0, FS_SEEK_SET);

  if (f == 0 || err == -1 || err2 == -1)
  {
    ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_http_load_payload unable to load file index.html, loading backup HTML.");

    int payload_len = sizeof(http_header_200) + sizeof(http_html_backup);
    state->http_payload_len = payload_len;
    state->http_payload_data = (char *) c_malloc(payload_len);
    if (state->http_payload_data == NULL)
    {
      return 2;
    }

    int offset = 0;
    c_memcpy(&(state->http_payload_data[offset]), &(http_header_200), sizeof(http_header_200));
    offset += sizeof(http_header_200);
    c_memcpy(&(state->http_payload_data[offset]), &(http_html_backup), sizeof(http_html_backup));

    return 1;
  }

  int payload_len = sizeof(http_header_200) + file_len;
  state->http_payload_len = payload_len;
  state->http_payload_data = (char *) c_malloc(payload_len);
  if (state->http_payload_data == NULL)
  {
    return 2;
  }

  int offset = 0;
  c_memcpy(&(state->http_payload_data[offset]), &(http_header_200), sizeof(http_header_200));
  offset += sizeof(http_header_200);
  fs_read(f, &(state->http_payload_data[offset]), file_len);

  return 0;
}


/**
 * De-escape URL data
 *
 * Parse escaped and form encoded data of request.
 */
static void enduser_setup_http_urldecode(char *dst, const char *src, int src_len)
{
  char a, b;
  int i;
  for (i = 0; i < src_len && *src; ++i)
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
}


/**
 * Handle HTTP Credentials
 *
 * @return - return 0 iff credentials are found and handled successfully
 *           return 1 iff credentials aren't found
 *           return 2 iff an error occured
 */
static int enduser_setup_http_handle_credentials(char *data, unsigned short data_len)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_http_handle_credentials");

  char *name_str = (char *) ((uint32_t)strstr(&(data[6]), "wifi_ssid="));
  char *pwd_str = (char *) ((uint32_t)strstr(&(data[6]), "wifi_password="));
  if (name_str == NULL || pwd_str == NULL)
  {
    return 1;
  }

  int name_field_len = sizeof("wifi_ssid=") - 1;
  int pwd_field_len = sizeof("wifi_password=") - 1;
  char *name_str_start = name_str + name_field_len;
  char *pwd_str_start = pwd_str + pwd_field_len;

  int name_str_len = enduser_setup_srch_str(name_str_start, "& ");
  int pwd_str_len = enduser_setup_srch_str(pwd_str_start, "& ");
  if (name_str_len == -1 || pwd_str_len == -1 || name_str_len > 31 || pwd_str_len > 63)
  {
    return 1;
  }

  struct station_config cnf;
  c_memset(&cnf, 0, sizeof(struct station_config));
  enduser_setup_http_urldecode(cnf.ssid, name_str_start, name_str_len);
  enduser_setup_http_urldecode(cnf.password, pwd_str_start, pwd_str_len);

  int err = wifi_set_opmode(STATION_MODE | wifi_get_opmode());
  if (err == FALSE)
  {
    wifi_set_opmode(~STATION_MODE & wifi_get_opmode());
    ENDUSER_SETUP_ERROR("enduser_setup_station_start failed. wifi_set_opmode failed.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }
  err = wifi_station_set_config(&cnf);
  if (err == FALSE)
  {
    wifi_set_opmode(~STATION_MODE & wifi_get_opmode());
    ENDUSER_SETUP_ERROR("enduser_setup_station_start failed. wifi_station_set_config failed.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }
  err = wifi_station_disconnect();
  if (err == FALSE)
  {
    ENDUSER_SETUP_ERROR_VOID("enduser_setup_station_start failed. wifi_station_disconnect failed.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
  }
  err = wifi_station_connect();
  if (err == FALSE)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_station_start failed. wifi_station_connect failed.\n", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }

  ENDUSER_SETUP_DEBUG(state->lua_L, "WiFi Credentials Stored");
  ENDUSER_SETUP_DEBUG(state->lua_L, "-----------------------");
  ENDUSER_SETUP_DEBUG(state->lua_L, "name: ");
  ENDUSER_SETUP_DEBUG(state->lua_L, cnf.ssid);
  ENDUSER_SETUP_DEBUG(state->lua_L, "pass: ");
  ENDUSER_SETUP_DEBUG(state->lua_L, cnf.password);
  ENDUSER_SETUP_DEBUG(state->lua_L, "-----------------------");

  return 0;
}


/**
 * Serve HTML
 *
 * @return - return 0 iff html was served successfully
 */
static int enduser_setup_http_serve_header(struct espconn *http_client, char *header, uint32_t header_len)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_http_serve_404");

  int8_t err = espconn_sent(http_client, header, header_len);
  if (err == ESPCONN_MEM)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_serve_header failed. espconn_send out of memory", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_NONFATAL);
  }
  else if (err == ESPCONN_ARG)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_serve_header failed. espconn_send can't find network transmission", ENDUSER_SETUP_ERR_CONNECTION_NOT_FOUND, ENDUSER_SETUP_ERR_NONFATAL);
  }
  else if (err != 0)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_serve_header failed. espconn_send failed", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
  }

  return 0;
}


/**
 * Serve HTML
 *
 * @return - return 0 iff html was served successfully
 */
static int enduser_setup_http_serve_html(struct espconn *http_client)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_http_serve_html");

  if (state->http_payload_data == NULL)
  {
    enduser_setup_http_load_payload();
  }

  int8_t err = espconn_sent(http_client, state->http_payload_data, state->http_payload_len);
  if (err == ESPCONN_MEM)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_serve_html failed. espconn_send out of memory", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_NONFATAL);
  }
  else if (err == ESPCONN_ARG)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_serve_html failed. espconn_send can't find network transmission", ENDUSER_SETUP_ERR_CONNECTION_NOT_FOUND, ENDUSER_SETUP_ERR_NONFATAL);
  }
  else if (err != 0)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_serve_html failed. espconn_send failed", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
  }

  return 0;
}


/**
 * Disconnect HTTP client
 *
 * End TCP connection and free up resources.
 */
static void enduser_setup_http_disconnect(struct espconn *espconn)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_http_disconnect");
//TODO: Construct and maintain os task queue(?) to be able to issue system_os_task with espconn_disconnect.
}


static void enduser_setup_http_recvcb(void *arg, char *data, unsigned short data_len)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_http_recvcb");
  struct espconn *http_client = (struct espconn *) arg;
  int retval;

  if (c_strncmp(data, "POST ", 5) == 0)
  {
    retval = enduser_setup_http_handle_credentials(data, data_len);
    if (retval == 0)
    {
      enduser_setup_http_serve_header(http_client, (char *) http_header_200, sizeof(http_header_200));
      enduser_setup_http_disconnect(http_client);
      return;
    }
    else if (retval == 2)
    {
      ENDUSER_SETUP_ERROR_VOID("enduser_setup_http_recvcb failed. Failed to handle wifi credentials.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
    }

    if (retval != 1)
    {
      ENDUSER_SETUP_ERROR_VOID("enduser_setup_http_recvcb failed. Unknown error code.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
    }
  }
  else if (c_strncmp(data, "GET ", 4) == 0)
  {
    /* Reject requests that probably aren't relevant to free up resources. */
    if (c_strncmp(data, "GET / ", 6) != 0)
    {
      ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_http_recvcb received too specific request.");
      enduser_setup_http_serve_header(http_client, (char *) http_header_404, sizeof(http_header_404));
      enduser_setup_http_disconnect(http_client);
      return;
    }

    retval = enduser_setup_http_serve_html(http_client);
    if (retval != 0)
    {
      enduser_setup_http_disconnect(http_client);
      ENDUSER_SETUP_ERROR_VOID("enduser_setup_http_recvcb failed. Unable to send HTML.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
    }
  }
  else
  {
    enduser_setup_http_serve_header(http_client, (char *) http_header_404, sizeof(http_header_404));
    enduser_setup_http_disconnect(http_client);
    return;
  }
}


static void enduser_setup_http_connectcb(void *arg)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_http_connectcb");
  struct espconn *callback_espconn = (struct espconn *) arg;

  int8_t err = 0;
  err |= espconn_regist_recvcb(callback_espconn, enduser_setup_http_recvcb);

  if (err != 0)
  {
    ENDUSER_SETUP_ERROR_VOID("enduser_setup_http_connectcb failed. Callback registration failed.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
  }
}


static int enduser_setup_http_start(void)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_http_start");
  state->espconn_http_tcp = (struct espconn *) c_malloc(sizeof(struct espconn));
  if (state->espconn_http_tcp == NULL)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_start failed. Memory allocation failed (espconn_http_tcp == NULL).", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }
  esp_tcp *esp_tcp_data = (esp_tcp *) c_malloc(sizeof(esp_tcp));
  if (esp_tcp_data == NULL)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_start failed. Memory allocation failed (esp_udp == NULL).", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }

  c_memset(state->espconn_http_tcp, 0, sizeof(struct espconn));
  c_memset(esp_tcp_data, 0, sizeof(esp_tcp));
  state->espconn_http_tcp->proto.tcp = esp_tcp_data;
  state->espconn_http_tcp->type = ESPCONN_TCP;
  state->espconn_http_tcp->state = ESPCONN_NONE;
  esp_tcp_data->local_port = 80;

  int8_t err;
  err = espconn_regist_connectcb(state->espconn_http_tcp, enduser_setup_http_connectcb);
  if (err != 0)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_start failed. Couldn't add receive callback.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }

  err = espconn_accept(state->espconn_http_tcp);
  if (err == ESPCONN_ISCONN)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_start failed. Couldn't create connection, already listening for that connection.", ENDUSER_SETUP_ERR_SOCKET_ALREADY_OPEN, ENDUSER_SETUP_ERR_FATAL);
  }
  else if (err == ESPCONN_MEM)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_start failed. Couldn't create connection, out of memory.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }
  else if (err == ESPCONN_ARG)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_start failed. Can't find connection from espconn argument", ENDUSER_SETUP_ERR_CONNECTION_NOT_FOUND, ENDUSER_SETUP_ERR_FATAL);
  }
  else if (err != 0)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_start failed. Unknown error", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }

  err = espconn_regist_time(state->espconn_http_tcp, 2, 0);
  if (err == ESPCONN_ARG)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_start failed. Unable to set TCP timeout.", ENDUSER_SETUP_ERR_CONNECTION_NOT_FOUND, ENDUSER_SETUP_ERR_NONFATAL | ENDUSER_SETUP_ERR_NO_RETURN);
  }

  err = enduser_setup_http_load_payload();
  if (err == 1)
  {
    ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_http_start info. Loaded backup HTML.");
  }
  else if (err == 2)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_http_start failed. Unable to allocate memory for HTTP payload.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }

  return 0;
}


static void enduser_setup_http_stop(void)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_http_stop");

  if (state != NULL && state->espconn_http_tcp != NULL)
  {
    espconn_delete(state->espconn_http_tcp);
  }
}

static void enduser_setup_ap_stop(void)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_station_stop");

  wifi_set_opmode(~SOFTAP_MODE & wifi_get_opmode());
}


static void enduser_setup_ap_start(void)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_ap_start");

  struct softap_config cnf;
  c_memset(&(cnf), 0, sizeof(struct softap_config));

  char ssid[] = ENDUSER_SETUP_AP_SSID;
  int ssid_name_len = c_strlen(ENDUSER_SETUP_AP_SSID);
  c_memcpy(&(cnf.ssid), ssid, ssid_name_len);

  uint8_t mac[6];
  wifi_get_macaddr(SOFTAP_IF, mac);
  cnf.ssid[ssid_name_len] = '_';
  cnf.ssid[ssid_name_len + 1] = (char) (65 +  (0x0F & mac[3]));
  cnf.ssid[ssid_name_len + 2] = (char) (65 + ((0xF0 & mac[3]) >> 4));
  cnf.ssid[ssid_name_len + 3] = (char) (65 +  (0x0F & mac[4]));
  cnf.ssid[ssid_name_len + 4] = (char) (65 + ((0xF0 & mac[4]) >> 4));
  cnf.ssid[ssid_name_len + 5] = (char) (65 +  (0x0F & mac[5]));
  cnf.ssid[ssid_name_len + 6] = (char) (65 + ((0xF0 & mac[5]) >> 4));

  cnf.ssid_len = c_strlen(ssid) + 7;
  cnf.channel = 1;
  cnf.authmode = AUTH_OPEN;
  cnf.ssid_hidden = 0;
  cnf.max_connection = 5;
  cnf.beacon_interval = 100;
  wifi_softap_set_config(&cnf);
  wifi_set_opmode(SOFTAP_MODE | wifi_get_opmode());
}


static void enduser_setup_dns_recv_callback(void *arg, char *recv_data, unsigned short recv_len)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_dns_recv_callback.");

  struct espconn *callback_espconn = arg;
  struct ip_info ip_info;

  uint32_t qname_len = c_strlen(&(recv_data[12])) + 1; // \0=1byte
  uint32_t dns_reply_static_len = (uint32_t) sizeof(dns_header) + (uint32_t) sizeof(dns_body) + 2 + 4; // dns_id=2bytes, ip=4bytes
  uint32_t dns_reply_len = dns_reply_static_len + qname_len;

  uint8_t if_mode = wifi_get_opmode();
  if ((if_mode & SOFTAP_MODE) == 0)
  {
    ENDUSER_SETUP_ERROR_VOID("enduser_setup_dns_recv_callback failed. Interface mode not supported.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }

  uint8_t if_index = (if_mode == STATION_MODE? STATION_IF : SOFTAP_IF);
  if (wifi_get_ip_info(if_index , &ip_info) == false)
  {
    ENDUSER_SETUP_ERROR_VOID("enduser_setup_dns_recv_callback failed. Unable to get interface IP.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }


  char *dns_reply = (char *) c_malloc(dns_reply_len);
  if (dns_reply == NULL)
  {
    ENDUSER_SETUP_ERROR_VOID("enduser_setup_dns_recv_callback failed. Failed to allocate memory.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_NONFATAL);
  }

  uint32_t insert_byte = 0;
  c_memcpy(&(dns_reply[insert_byte]), recv_data, 2);
  insert_byte += 2;
  c_memcpy(&(dns_reply[insert_byte]), dns_header, sizeof(dns_header));
  insert_byte += (uint32_t) sizeof(dns_header);
  c_memcpy(&(dns_reply[insert_byte]), &(recv_data[12]), qname_len);
  insert_byte += qname_len;
  c_memcpy(&(dns_reply[insert_byte]), dns_body, sizeof(dns_body));
  insert_byte += (uint32_t) sizeof(dns_body);
  c_memcpy(&(dns_reply[insert_byte]), &(ip_info.ip), 4);

  // SDK 1.4.0 changed behaviour, for UDP server need to look up remote ip/port
  remot_info *pr = 0;
  if (espconn_get_connection_info(callback_espconn, &pr, 0) != ESPCONN_OK)
  {
    ENDUSER_SETUP_ERROR_VOID("enduser_setup_dns_recv_callback failed. Unable to get IP of UDP sender.", ENDUSER_SETUP_ERR_CONNECTION_NOT_FOUND, ENDUSER_SETUP_ERR_FATAL);
  }
  callback_espconn->proto.udp->remote_port = pr->remote_port;
  os_memmove(callback_espconn->proto.udp->remote_ip, pr->remote_ip, 4);

  int8_t err;
  err = espconn_sent(callback_espconn, dns_reply, dns_reply_len);
  c_free(dns_reply);
  if (err == ESPCONN_MEM)
  {
    ENDUSER_SETUP_ERROR_VOID("enduser_setup_dns_recv_callback failed. Failed to allocate memory for send.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }
  else if (err == ESPCONN_ARG)
  {
    ENDUSER_SETUP_ERROR_VOID("enduser_setup_dns_recv_callback failed. Can't execute transmission.", ENDUSER_SETUP_ERR_CONNECTION_NOT_FOUND, ENDUSER_SETUP_ERR_FATAL);
  }
  else if (err != 0)
  {
    ENDUSER_SETUP_ERROR_VOID("enduser_setup_dns_recv_callback failed. espconn_send failed", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }
}


static void enduser_setup_free(void)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_free");

  if (state == NULL)
  {
    return;
  }

  if (state->espconn_dns_udp != NULL)
  {
    if (state->espconn_dns_udp->proto.udp != NULL)
    {
      c_free(state->espconn_dns_udp->proto.udp);
    }
    c_free(state->espconn_dns_udp);
  }

  if (state->espconn_http_tcp != NULL)
  {
    if (state->espconn_http_tcp->proto.tcp != NULL)
    {
      c_free(state->espconn_http_tcp->proto.tcp);
    }
    c_free(state->espconn_http_tcp);
  }
  c_free(state->http_payload_data);
  c_free(state);
  state = NULL;
}


static int enduser_setup_dns_start(void)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_dns_start");

  if (state->espconn_dns_udp != NULL)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_dns_start failed. Appears to already be started (espconn_dns_udp != NULL).", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }
  state->espconn_dns_udp = (struct espconn *) c_malloc(sizeof(struct espconn));
  if (state->espconn_dns_udp == NULL)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_dns_start failed. Memory allocation failed (espconn_dns_udp == NULL).", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }

  esp_udp *esp_udp_data = (esp_udp *) c_malloc(sizeof(esp_udp));
  if (esp_udp_data == NULL)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_dns_start failed. Memory allocation failed (esp_udp == NULL).", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }

  c_memset(state->espconn_dns_udp, 0, sizeof(struct espconn));
  c_memset(esp_udp_data, 0, sizeof(esp_udp));
  state->espconn_dns_udp->proto.udp = esp_udp_data;
  state->espconn_dns_udp->type = ESPCONN_UDP;
  state->espconn_dns_udp->state = ESPCONN_NONE;
  esp_udp_data->local_port = 53;

  int8_t err;
  err = espconn_regist_recvcb(state->espconn_dns_udp, enduser_setup_dns_recv_callback);
  if (err != 0)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_dns_start failed. Couldn't add receive callback, unknown error.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }

  err = espconn_create(state->espconn_dns_udp);
  if (err == ESPCONN_ISCONN)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_dns_start failed. Couldn't create connection, already listening for that connection.", ENDUSER_SETUP_ERR_SOCKET_ALREADY_OPEN, ENDUSER_SETUP_ERR_FATAL);
  }
  else if (err == ESPCONN_MEM)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_dns_start failed. Couldn't create connection, out of memory.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }
  else if (err != 0)
  {
    ENDUSER_SETUP_ERROR("enduser_setup_dns_start failed. Couldn't create connection, unknown error.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }

  return 0;
}


static void enduser_setup_dns_stop(void)
{
  ENDUSER_SETUP_DEBUG(state->lua_L, "enduser_setup_dns_stop");

  if (state->espconn_dns_udp != NULL)
  {
    espconn_delete(state->espconn_dns_udp);
  }
}


static int enduser_setup_init(lua_State *L)
{
  ENDUSER_SETUP_DEBUG(L, "enduser_setup_init");

  if (state != NULL)
  {
    enduser_setup_error(L, "enduser_setup_init failed. Appears to already be started.", ENDUSER_SETUP_ERR_UNKOWN_ERROR);
    return ENDUSER_SETUP_ERR_UNKOWN_ERROR;
  }

  state = (enduser_setup_state_t *) os_malloc(sizeof(enduser_setup_state_t));
  if (state == NULL)
  {
    enduser_setup_error(L, "enduser_setup_init failed. Unable to allocate memory.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY);
    return ENDUSER_SETUP_ERR_OUT_OF_MEMORY;
  }
  c_memset(state, 0, sizeof(enduser_setup_state_t));

  state->lua_L = L;
  state->lua_connected_cb_ref = LUA_NOREF;
  state->lua_err_cb_ref = LUA_NOREF;
  state->lua_dbg_cb_ref = LUA_NOREF;

  if (!lua_isnoneornil(L, 1))
  {
    lua_pushvalue(L, 1);
    state->lua_connected_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else
  {
    state->lua_connected_cb_ref = LUA_NOREF;
  }

  if (!lua_isnoneornil(L, 2))
  {
    lua_pushvalue (L, 2);
    state->lua_err_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else
  {
    state->lua_err_cb_ref = LUA_NOREF;
  }

  if (!lua_isnoneornil(L, 3))
  {
    lua_pushvalue (L, 3);
    state->lua_dbg_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else
  {
    state->lua_dbg_cb_ref = LUA_NOREF;
  }

  return 0;
}


static int enduser_setup_start(lua_State *L)
{
  ENDUSER_SETUP_DEBUG(L, "enduser_setup_start");

  if(enduser_setup_init(L))
  {
    enduser_setup_stop(L);
    return 0;
  }

  enduser_setup_check_station_start();
  enduser_setup_ap_start();

  if(enduser_setup_dns_start())
  {
    enduser_setup_stop(L);
    return 0;
  }

  if(enduser_setup_http_start())
  {
    enduser_setup_stop(L);
    return 0;
  }

  return 0;
}


static int enduser_setup_stop(lua_State* L)
{
  enduser_setup_check_station_stop();
  enduser_setup_ap_stop();
  enduser_setup_dns_stop();
  enduser_setup_http_stop();
  enduser_setup_free();

  return 0;
}


#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE enduser_setup_map[] =
{
  { LSTRKEY( "start" ), LFUNCVAL( enduser_setup_start )},
  { LSTRKEY( "stop" ),  LFUNCVAL( enduser_setup_stop  )},
  { LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_enduser_setup(lua_State *L) {
  LREGISTER(L, "enduser_setup", enduser_setup_map);
  return 1;
}

