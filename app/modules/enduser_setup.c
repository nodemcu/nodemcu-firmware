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
 */

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_stdio.h"
#include "c_string.h"
#include "ctype.h"
#include "user_interface.h"
#include "espconn.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "flash_fs.h"
#include "task/task.h"

#define MIN(x, y)  (((x) < (y)) ? (x) : (y))
#define LITLEN(strliteral) (sizeof (strliteral) -1)

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

static const char http_html_filename[] = "enduser_setup.html";
static const char http_header_200[] = "HTTP/1.1 200 OK\r\nCache-control:no-cache\r\nContent-Type: text/html\r\n"; /* Note single \r\n here! */
static const char http_header_204[] = "HTTP/1.1 204 No Content\r\n\r\n";
static const char http_header_302[] = "HTTP/1.1 302 Moved\r\nLocation: /\r\n\r\n";
static const char http_header_401[] = "HTTP/1.1 401 Bad request\r\n\r\n";
static const char http_header_404[] = "HTTP/1.1 404 Not found\r\n\r\n";
static const char http_header_500[] = "HTTP/1.1 500 Internal Error\r\n\r\n";

/* The below is the un-minified version of the http_html_backup[] string.
 * Minified using https://kangax.github.io/html-minifier/
 * Note: using method="get" due to iOS not always sending body in same
 *       packet as the HTTP header, and us thus missing it in that case
 */
#if 0
<!DOCTYPE html>
<html>
<head>
  <meta http-equiv="content-type" content="text/html; charset=UTF-8">
  <meta charset="utf-8">
  <meta name="viewport" content="width=380">
  <title>WiFi Login</title>
  <style media="screen" type="text/css">
    *{margin:0;padding:0}
    html{height:100%;background:linear-gradient(rgba(196,102,0,.2),rgba(155,89,182,.2)),url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEYAAAA8AgMAAACm+SSwAAAADFBMVEVBR1FFS1VHTlg8Q0zU/YXIAAADVElEQVQ4yy1TTYvTUBQ9GTKiYNoodsCF4MK6U4TZChOhiguFWHyBFzqlLl4hoeNvEBeCrlrhBVKq1EUKLTP+hvi1GyguXqBdiZCBzGqg20K8L3hDQnK55+OeJNguHx6UujYl3dL5ALn4JOIUluAqeAWciyGaSdvngOWzNT+G0UyGUOxVOAdqkjXDCbBiUyjZ5QzYEbGadYAi6kHxth+kthXNVNCDofwhGv1D4QGGiM9iAjbCHgr2iUUpDJbs+VPQ4xAr2fX7KXbkOJMdok965Ksb+6lrjdkem8AshIuHm9Nyu19uTunYlOXDTQqi8VgeH0kBXH2xq/ouiMZPzuMukymutrBmulUTovC6HqNFW2ZOiqlpSXZOTvSUeUPxChjxol8BLbRy4gJuhV7OR4LRVBs3WQ9VVAU7SXgK2HeUrOj7bC8YsUgr3lEV/TXB7hK90EBnxaeg1Ov15bY80M736ekCGesGAaGvG0Ct4WRkVQVHIgIM9xJgvSFfPay8Q6GNv7VpR7xUnkvhnMQCJDYkYOtNLihV70tCU1Sk+BQrpoP+HLHUrJkuta40C6LP5GvBv+Hqo10ATxxFrTPvNdPr7XwgQud6RvQN/sXjBGzqbU27wcj9cgsyvSTrpyXV8gKpXeNJU3aFl7MOdldzV4+HfO19jBa5f2IjWwx1OLHIvFHkqbBj20ro1g7nDfY1DpScvDRUNARgjMMVO0zoMjKxJ6uWCPP+YRAWbGoaN8kXYHmLjB9FXLGOazfFVCvOgqzfnicNPrHtPKlex2ye824gMza0cTZ2sS2Xm7Qst/UfFw8O6vVtmUKxZy9xFgzMys5cJ5fxZw4y37Ufk1Dsfb8MqOjYxE3ZMWxiDcO0PYUaD2ys+8OW1pbB7/e3sfZeGVCL0Q2aMjjPdm2sxADuejZxHJAd8dO9DSUdA0V8/NggRRanDkBrANn8yHlEQOn/MmwoQfQF7xgmKDnv520bS/pgylP67vf3y2V5sCwfoCEMkZClgOfJAFX9eXefR2RpnmRs4CDVPceaRfoFzCkJVJX27vWZnoqyvmtXU3+dW1EIXIu8Qg5Qta4Zlv7drUCoWe8/8MXzaEwux7ESE9h6qnHj3mIO0/D9RvzfxPmjWiQ1vbeSk4rrHwhAre35EEVaAAAAAElFTkSuQmCC)}
    body{font-family:arial,verdana}
    div{position:absolute;margin:auto;top:-150px;right:0;bottom:0;left:0;width:320px;height:304px}
    form{width:320px;text-align:center;position:relative}
    form fieldset{background:#fff;border:0 none;border-radius:5px;box-shadow:0 0 15px 1px rgba(0,0,0,.4);padding:20px 30px;box-sizing:border-box}
    form input{padding:15px;border:1px solid #ccc;border-radius:3px;margin-bottom:10px;width:100%;box-sizing:border-box;font-family:montserrat;color:#2C3E50;font-size:13px}
    form .action-button{border:0 none;border-radius:3px;cursor:pointer;}
    #msform .submit:focus,form .action-button:hover{box-shadow:0 0 0 2px #fff,0 0 0 3px #27AE60;}
    #formFrame{display: none;}
    #aplist{display: block;}
    select{width:100%;margin-bottom: 20px;padding: 10px 5px; border:1px solid #ccc;display:none;}
    .fs-title{font-size:15px;text-transform:uppercase;color:#2C3E50;margin-bottom:10px}
    .fs-subtitle{font-weight:400;font-size:13px;color:#666;margin-bottom:20px}
    .fs-status{font-weight:400;font-size:13px;color:#666;margin-bottom:10px;padding-top:20px; border-top:1px solid #ccc}
    .submit{width:100px;background: #27AE60; color: #fff;font-weight:700;margin:10px 5px; padding: 10px 5px; }
  </style>
</head>
<body>
  <div>
    <form id="credentialsForm" method="get" action="/update" target="formFrame">
      <fieldset>
        <iframe id="formFrame" src="" name="formFrame"></iframe> <!-- Used to submit data, needed to prevent re-direction after submission -->
        <h2 class="fs-title">WiFi Login</h2>
        <h3 class="fs-subtitle">Connect gadget to your WiFi network</h3>
        <input id="wifi_ssid" autocorrect="off" autocapitalize="none" name="wifi_ssid" placeholder="WiFi Name">
        <select id="aplist" name="aplist" size="1" disabled>
          <option>Scanning for networks...</option>
        </select>
        <input name="wifi_password" placeholder="Password" type="password">
        <input type=submit name=save class="action-button submit" value="Save">
        <h3 class="fs-status">Status: <span id="status">Updating...</span></h3>
      </fieldset>
      <h3 id="dbg"></h3>
    </form>
  </div>
  <script>
    function fetch(url, method, callback)
    {
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange=check_ready;
      function check_ready()
      {
        if (xhr.readyState === 4)
        {
          callback(xhr.status === 200 ? xhr.responseText : null);
        }
      }
      xhr.open(method, url, true);
      xhr.send();
    }


    function new_status(stat)
    {
      if (stat)
      {
        var e = document.getElementById("status");
        e.innerHTML = stat;
      }
    }


    function new_status_repeat(stat)
    {
      new_status(stat);
      setTimeout(refresh_status, 750);
    }


    function new_ap_list(json)
    {
      if (json)
      {
        var list = JSON.parse(json);
        list.sort(function(a, b){ return b.rssi - a.rssi; });
        var ssids = list.map(function(a) { return a.ssid; }).filter(function(item, pos, self) { return self.indexOf(item)==pos; });
        var sel = document.getElementById("aplist");
        sel.innerHTML = "";
        sel.setAttribute("size", Math.max(Math.min(3, list.length), 1));
        sel.removeAttribute("disabled");
        for (var i = 0; i < ssids.length; ++i)
        {
          var o = document.createElement("option");
          o.innerHTML = ssids[i];
          sel.options.add(o);
        }
        sel.style.display = 'block';
      }
    }


    function new_ap_list_repeat(json)
    {
      new_ap_list(json);
      setTimeout(refresh_ap_list, 3000);
    }


    function refresh_status()
    {
      fetch('/status','GET', new_status_repeat);
    }


    function refresh_ap_list()
    {
      fetch('/aplist','GET', new_ap_list_repeat);
    }

    function set_ssid_field() {
        var sel = document.getElementById("aplist");
        document.getElementById("wifi_ssid").value = sel.value;
    }

    window.onload = function()
    {
      refresh_status();
      refresh_ap_list();
      document.getElementById("aplist").onclick = set_ssid_field;
      document.getElementById("aplist").onchange = set_ssid_field;
      document.getElementById("credentialsForm").addEventListener("submit", function(){
        fetch('/status','GET', new_status);
      });
    }
  </script>
</body>
</html>
#endif
static const char http_html_backup[] =
"<!DOCTYPE html><meta http-equiv=content-type content='text/html; charset=UTF-8'><meta charset=utf-8><meta name=viewport content='width=380'><title>WiFi Login</title><style media=screen type=text/css>*{margin:0;padding:0}html{height:100%;background:linear-gradient(rgba(196,102,0,.2),rgba(155,89,182,.2)),url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEYAAAA8AgMAAACm+SSwAAAADFBMVEVBR1FFS1VHTlg8Q0zU/YXIAAADVElEQVQ4yy1TTYvTUBQ9GTKiYNoodsCF4MK6U4TZChOhiguFWHyBFzqlLl4hoeNvEBeCrlrhBVKq1EUKLTP+hvi1GyguXqBdiZCBzGqg20K8L3hDQnK55+OeJNguHx6UujYl3dL5ALn4JOIUluAqeAWciyGaSdvngOWzNT+G0UyGUOxVOAdqkjXDCbBiUyjZ5QzYEbGadYAi6kHxth+kthXNVNCDofwhGv1D4QGGiM9iAjbCHgr2iUUpDJbs+VPQ4xAr2fX7KXbkOJMdok965Ksb+6lrjdkem8AshIuHm9Nyu19uTunYlOXDTQqi8VgeH0kBXH2xq/ouiMZPzuMukymutrBmulUTovC6HqNFW2ZOiqlpSXZOTvSUeUPxChjxol8BLbRy4gJuhV7OR4LRVBs3WQ9VVAU7SXgK2HeUrOj7bC8YsUgr3lEV/TXB7hK90EBnxaeg1Ov15bY80M736ekCGesGAaGvG0Ct4WRkVQVHIgIM9xJgvSFfPay8Q6GNv7VpR7xUnkvhnMQCJDYkYOtNLihV70tCU1Sk+BQrpoP+HLHUrJkuta40C6LP5GvBv+Hqo10ATxxFrTPvNdPr7XwgQud6RvQN/sXjBGzqbU27wcj9cgsyvSTrpyXV8gKpXeNJU3aFl7MOdldzV4+HfO19jBa5f2IjWwx1OLHIvFHkqbBj20ro1g7nDfY1DpScvDRUNARgjMMVO0zoMjKxJ6uWCPP+YRAWbGoaN8kXYHmLjB9FXLGOazfFVCvOgqzfnicNPrHtPKlex2ye824gMza0cTZ2sS2Xm7Qst/UfFw8O6vVtmUKxZy9xFgzMys5cJ5fxZw4y37Ufk1Dsfb8MqOjYxE3ZMWxiDcO0PYUaD2ys+8OW1pbB7/e3sfZeGVCL0Q2aMjjPdm2sxADuejZxHJAd8dO9DSUdA0V8/NggRRanDkBrANn8yHlEQOn/MmwoQfQF7xgmKDnv520bS/pgylP67vf3y2V5sCwfoCEMkZClgOfJAFX9eXefR2RpnmRs4CDVPceaRfoFzCkJVJX27vWZnoqyvmtXU3+dW1EIXIu8Qg5Qta4Zlv7drUCoWe8/8MXzaEwux7ESE9h6qnHj3mIO0/D9RvzfxPmjWiQ1vbeSk4rrHwhAre35EEVaAAAAAElFTkSuQmCC)}body{font-family:arial,verdana}div{position:absolute;margin:auto;top:-150px;right:0;bottom:0;left:0;width:320px;height:304px}form{width:320px;text-align:center;position:relative}form fieldset{background:#fff;border:0 none;border-radius:5px;box-shadow:0 0 15px 1px rgba(0,0,0,.4);padding:20px 30px;box-sizing:border-box}form input{padding:15px;border:1px solid #ccc;border-radius:3px;margin-bottom:10px;width:100%;box-sizing:border-box;font-family:montserrat;color:#2C3E50;font-size:13px}form .action-button{border:0 none;border-radius:3px;cursor:pointer}#msform .submit:focus,form .action-button:hover{box-shadow:0 0 0 2px #fff,0 0 0 3px #27AE60}#formFrame{display:none}#aplist{display:block}select{width:100%;margin-bottom:20px;padding:10px 5px;border:1px solid #ccc;display:none}.fs-title{font-size:15px;text-transform:uppercase;color:#2C3E50;margin-bottom:10px}.fs-subtitle{font-weight:400;font-size:13px;color:#666;margin-bottom:20px}.fs-status{font-weight:400;font-size:13px;color:#666;margin-bottom:10px;padding-top:20px;border-top:1px solid #ccc}.submit{width:100px;background:#27AE60;color:#fff;font-weight:700;margin:10px 5px;padding:10px 5px}</style><div><form id=credentialsForm action=/update target=formFrame><fieldset><iframe id=formFrame src=''name=formFrame></iframe><h2 class=fs-title>WiFi Login</h2><h3 class=fs-subtitle>Connect gadget to your WiFi network</h3><input id=wifi_ssid autocorrect=off autocapitalize=none name=wifi_ssid placeholder='WiFi Name'><select id=aplist name=aplist size=1 disabled><option>Scanning for networks...</select><input name=wifi_password placeholder=Password type=password> <input type=submit name=save class='action-button submit'value=Save><h3 class=fs-status>Status: <span id=status>Updating...</span></h3></fieldset><h3 id=dbg></h3></form></div><script>function fetch(t,e,n){function s(){4===i.readyState&&n(200===i.status?i.responseText:null)}var i=new XMLHttpRequest;i.onreadystatechange=s,i.open(e,t,!0),i.send()}function new_status(t){if(t){var e=document.getElementById('status');e.innerHTML=t}}function new_status_repeat(t){new_status(t),setTimeout(refresh_status,750)}function new_ap_list(t){if(t){var e=JSON.parse(t);e.sort(function(t,e){return e.rssi-t.rssi});var n=e.map(function(t){return t.ssid}).filter(function(t,e,n){return n.indexOf(t)==e}),s=document.getElementById('aplist');s.innerHTML='',s.setAttribute('size',Math.max(Math.min(3,e.length),1)),s.removeAttribute('disabled');for(var i=0;i<n.length;++i){var a=document.createElement('option');a.innerHTML=n[i],s.options.add(a)}s.style.display='block'}}function new_ap_list_repeat(t){new_ap_list(t),setTimeout(refresh_ap_list,3e3)}function refresh_status(){fetch('/status','GET',new_status_repeat)}function refresh_ap_list(){fetch('/aplist','GET',new_ap_list_repeat)}function set_ssid_field(){var t=document.getElementById('aplist');document.getElementById('wifi_ssid').value=t.value}window.onload=function(){refresh_status(),refresh_ap_list(),document.getElementById('aplist').onclick=set_ssid_field,document.getElementById('aplist').onchange=set_ssid_field,document.getElementById('credentialsForm').addEventListener('submit',function(){fetch('/status','GET',new_status)})}</script>";


typedef struct scan_listener
{
  struct tcp_pcb *conn;
  struct scan_listener *next;
} scan_listener_t;

typedef struct
{
  struct espconn *espconn_dns_udp;
  struct tcp_pcb *http_pcb;
  char *http_payload_data;
  uint32_t http_payload_len;
  os_timer_t check_station_timer;
  os_timer_t shutdown_timer;
  int lua_connected_cb_ref;
  int lua_err_cb_ref;
  int lua_dbg_cb_ref;
  scan_listener_t *scan_listeners;
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


#define ENDUSER_SETUP_DEBUG_ENABLE 0
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
    lua_call(L, 1, 0);
  }
}


static void enduser_setup_error(int line, const char *str, int err)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_error");

  lua_State *L = lua_getstate();
  if (state != NULL && state->lua_err_cb_ref != LUA_NOREF)
  {
    lua_rawgeti (L, LUA_REGISTRYINDEX, state->lua_err_cb_ref);
    lua_pushnumber(L, err);
    lua_pushfstring(L, "%d: \t%s", line, str);
    lua_call (L, 2, 0);
  }
}


static void enduser_setup_connected_callback()
{
  ENDUSER_SETUP_DEBUG("enduser_setup_connected_callback");

  lua_State *L = lua_getstate();
  if (state != NULL && state->lua_connected_cb_ref != LUA_NOREF)
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, state->lua_connected_cb_ref);
    lua_call(L, 0, 0);
  }
}


static void enduser_setup_check_station_start(void)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_check_station_start");

  os_timer_setfn(&(state->check_station_timer), enduser_setup_check_station, NULL);
  os_timer_arm(&(state->check_station_timer), 1*1000, TRUE);
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

  enduser_setup_check_station_stop();
  enduser_setup_connected_callback();

  /* Trigger shutdown, but allow time for HTTP client to fetch last status. */
  if (!manual)
  {
    os_timer_setfn(&(state->shutdown_timer), enduser_setup_stop_callback, NULL);
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
  (void)arg;
  tcp_poll (pcb, 0, 0);
  tcp_abort (pcb);
  return ERR_ABRT;
}

/* Callback to detect a remote-close of a connection */
static err_t handle_remote_close (void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
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
  tcp_poll (pcb, force_abort, 15); /* ~3sec from now */
  tcp_recv (pcb, handle_remote_close);
  tcp_sent (pcb, 0);
}

/* Convenience function to queue up a close-after-send.  */
static err_t close_once_sent (void *arg, struct tcp_pcb *pcb, u16_t len)
{
  (void)arg; (void)len;
  deferred_close (pcb);
  return ERR_OK;
}

/* ------------------------------------------------------------------------- */

/**
 * Search String
 *
 * Search string for first occurance of any char in srch_str.
 *
 * @return -1 iff no occurance of char was found.
 */
static int enduser_setup_srch_str(const char *str, const char *srch_str)
{
  char *found = strpbrk (str, srch_str);
  if (!found)
  {
    return -1;
  }
  else
  {
    return found - str;
  }
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
  ENDUSER_SETUP_DEBUG("enduser_setup_http_load_payload");

  int err = (FS_OPEN_OK -1);
  int err2 = (FS_OPEN_OK -1);
  int file_len = 0;
  int f = fs_open(http_html_filename, fs_mode2flag("r"));
  if (f >= FS_OPEN_OK) {
      err = fs_seek(f, 0, FS_SEEK_END);
      file_len = (int) fs_tell(f);
      err2 = fs_seek(f, 0, FS_SEEK_SET);
  }

  const char cl_hdr[] = "Content-length:%5d\r\n\r\n";
  const size_t cl_len = LITLEN(cl_hdr) + 3; /* room to expand %4d */

  if (f < FS_OPEN_OK || err < FS_OPEN_OK || err2 < FS_OPEN_OK)
  {
    ENDUSER_SETUP_DEBUG("enduser_setup_http_load_payload unable to load file enduser_setup.html, loading backup HTML.");

    int payload_len = LITLEN(http_header_200) + cl_len + LITLEN(http_html_backup);
    state->http_payload_len = payload_len;
    state->http_payload_data = (char *) c_malloc(payload_len);
    if (state->http_payload_data == NULL)
    {
      return 2;
    }

    int offset = 0;
    c_memcpy(&(state->http_payload_data[offset]), &(http_header_200), LITLEN(http_header_200));
    offset += LITLEN(http_header_200);
    offset += c_sprintf(state->http_payload_data + offset, cl_hdr, LITLEN(http_html_backup));
    c_memcpy(&(state->http_payload_data[offset]), &(http_html_backup), LITLEN(http_html_backup));

    return 1;
  }

  int payload_len = LITLEN(http_header_200) + cl_len + file_len;
  state->http_payload_len = payload_len;
  state->http_payload_data = (char *) c_malloc(payload_len);
  if (state->http_payload_data == NULL)
  {
    return 2;
  }

  int offset = 0;
  c_memcpy(&(state->http_payload_data[offset]), &(http_header_200), LITLEN(http_header_200));
  offset += LITLEN(http_header_200);
  offset += c_sprintf(state->http_payload_data + offset, cl_hdr, file_len);
  fs_read(f, &(state->http_payload_data[offset]), file_len);
  fs_close(f);

  return 0;
}


/**
 * De-escape URL data
 *
 * Parse escaped and form encoded data of request.
 *
 * @return - return 0 iff the HTTP parameter is decoded into a valid string.
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
 * Handle HTTP Credentials
 *
 * @return - return 0 iff credentials are found and handled successfully
 *           return 1 iff credentials aren't found
 *           return 2 iff an error occured
 */
static int enduser_setup_http_handle_credentials(char *data, unsigned short data_len)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_http_handle_credentials");

  char *name_str = (char *) ((uint32_t)strstr(&(data[6]), "wifi_ssid="));
  char *pwd_str = (char *) ((uint32_t)strstr(&(data[6]), "wifi_password="));
  if (name_str == NULL || pwd_str == NULL)
  {
    ENDUSER_SETUP_DEBUG("Password or SSID string not found");
    return 1;
  }

  int name_field_len = LITLEN("wifi_ssid=");
  int pwd_field_len = LITLEN("wifi_password=");
  char *name_str_start = name_str + name_field_len;
  char *pwd_str_start = pwd_str + pwd_field_len;

  int name_str_len = enduser_setup_srch_str(name_str_start, "& ");
  int pwd_str_len = enduser_setup_srch_str(pwd_str_start, "& ");
  if (name_str_len == -1 || pwd_str_len == -1)
  {
    ENDUSER_SETUP_DEBUG("Password or SSID HTTP paramter divider not found");
    return 1;
  }


  struct station_config *cnf = luaM_malloc(lua_getstate(), sizeof(struct station_config));
  c_memset(cnf, 0, sizeof(struct station_config));

  int err;
  err  = enduser_setup_http_urldecode(cnf->ssid, name_str_start, name_str_len, sizeof(cnf->ssid));
  err |= enduser_setup_http_urldecode(cnf->password, pwd_str_start, pwd_str_len, sizeof(cnf->password));
  if (err != 0)
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
 * @return - return 0 iff html was served successfully
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
  }
  else
    tcp_arg (pcb, (void *)offs);

  return ERR_OK;
}


/**
 * Serve HTML
 *
 * @return - return 0 iff html was served successfully
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


static void serve_status(struct tcp_pcb *conn)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_serve_status");

  const char fmt[] =
    "HTTP/1.1 200 OK\r\n"
    "Cache-control:no-cache\r\n"
    "Connection:close\r\n"
    "Content-type:text/plain\r\n"
    "Content-length: %d\r\n"
    "\r\n"
    "%s%s";
  const char *state[] =
  {
    "Idle.",
    "Connecting to \"%s\".",
    "Failed to connect to \"%s\" - Wrong password.",
    "Failed to connect to \"%s\" - Network not found.",
    "Failed to connect.",
    "Connected to \"%s\" (%s)."
  };

  const size_t num_states = sizeof(state)/sizeof(state[0]);
  uint8_t curr_state = wifi_station_get_connect_status ();
  if (curr_state < num_states)
  {
    switch (curr_state)
    {
      case STATION_CONNECTING:
      case STATION_WRONG_PASSWORD:
      case STATION_NO_AP_FOUND:
      case STATION_GOT_IP:
      {
        const char *s = state[curr_state];
        struct station_config config;
        wifi_station_get_config(&config);
        config.ssid[31] = '\0';

	struct ip_info ip_info;

	wifi_get_ip_info(STATION_IF , &ip_info);

	char ip_addr[16];
	ip_addr[0] = '\0';
	if (curr_state == STATION_GOT_IP)
	{
		c_sprintf (ip_addr, "%d.%d.%d.%d", IP2STR(&ip_info.ip.addr));
	}


        int state_len = c_strlen(s);
        int ip_len = c_strlen(ip_addr);
        int ssid_len = c_strlen(config.ssid);
        int status_len = state_len + ssid_len + ip_len + 1;
        char status_buf[status_len];
        memset(status_buf, 0, status_len);
        status_len = c_sprintf(status_buf, s, config.ssid, ip_addr);

        int buf_len = sizeof(fmt) + status_len + 10; //10 = (9+1), 1 byte is '\0' and 9 are reserved for length field
        char buf[buf_len];
        memset(buf, 0, buf_len);
        int output_len = c_sprintf(buf, fmt, status_len, status_buf);

        enduser_setup_http_serve_header(conn, buf, output_len);
      }
      break;

      /* Handle non-formatted strings */
      default:
      {
        const char *s = state[curr_state];
        int status_len = c_strlen(s);
        int buf_len = sizeof(fmt) + status_len + 10; //10 = (9+1), 1 byte is '\0' and 9 are reserved for length field
        char buf[buf_len];
        memset(buf, 0, buf_len);
        int output_len = c_sprintf(buf, fmt, status_len, s);

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



/* --- WiFi AP scanning support -------------------------------------------- */

static void free_scan_listeners (void)
{
  if (!state || !state->scan_listeners)
  {
    return;
  }

  scan_listener_t *l = state->scan_listeners , *next = 0;
  while (l)
  {
    next = l->next;
    c_free (l);
    l = next;
  }
  state->scan_listeners = 0;
}


static void remove_scan_listener (scan_listener_t *l)
{
  if (state)
  {
    scan_listener_t **sl = &state->scan_listeners;
    while (*sl)
    {
      /* Remove any and all references to the closed conn from the scan list */
      if (*sl == l)
      {
        *sl = l->next;
        c_free (l);
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
      "Content-type:application/json\r\n"
      "Content-length:%4d\r\n"
      "\r\n";
    const size_t hdr_sz = sizeof (header_fmt) +1 -1; /* +expand %4d, -\0 */

    /* To be able to safely escape a pathological SSID, we need 2*32 bytes */
    const size_t max_entry_sz = sizeof("{\"ssid\":\"\",\"rssi\":},") + 2*32 + 6;
    const size_t alloc_sz = hdr_sz + num_nets * max_entry_sz + 3;
    char *http = os_zalloc (alloc_sz);
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

      p += c_sprintf (p, "%d", wn->rssi);

      *p++ = '}';
    }
    *p++ = ']';

    size_t body_sz = (p - http) - hdr_sz;
    c_sprintf (http, header_fmt, body_sz);
    http[hdr_sz] = '['; /* Rewrite the \0 with the correct start of body */

    notify_scan_listeners (http, hdr_sz + body_sz);
    c_free (http);
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

  if (!p) /* remote side closed, close our end too */
  {
    ENDUSER_SETUP_DEBUG("connection closed");
    scan_listener_t *l = arg; /* if it's waiting for scan, we have a ptr here */
    if (l)
      remove_scan_listener (l);

    deferred_close (http_client);
    return ERR_OK;
  }

  char *data = os_zalloc (p->tot_len + 1);
  if (!data)
    return ERR_MEM;

  unsigned data_len = pbuf_copy_partial (p, data, p->tot_len, 0);
  tcp_recved (http_client, p->tot_len);
  pbuf_free (p);

  err_t ret = ERR_OK;
  if (c_strncmp(data, "GET ", 4) == 0)
  {
    if (c_strncmp(data + 4, "/ ", 2) == 0)
    {
      if (enduser_setup_http_serve_html(http_client) != 0)
      {
        ENDUSER_SETUP_ERROR("http_recvcb failed. Unable to send HTML.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
      }
      else
      {
        goto free_out; /* streaming now in progress */
      }
    }
    else if (c_strncmp(data + 4, "/aplist ", 8) == 0)
    {
      scan_listener_t *l = os_malloc (sizeof (scan_listener_t));
      if (!l)
      {
        ENDUSER_SETUP_ERROR("out of memory", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_NONFATAL);
      }

      bool already = (state->scan_listeners != NULL);

      tcp_arg (http_client, l);
      /* TODO: check if also need a tcp_err() cb, or if recv() is enough */
      l->conn = http_client;
      l->next = state->scan_listeners;
      state->scan_listeners = l;

      if (!already)
      {
        if (!wifi_station_scan(NULL, on_scan_done))
        {
          enduser_setup_http_serve_header(http_client, http_header_500, LITLEN(http_header_500));
          deferred_close (l->conn);
          l->conn = 0;
          free_scan_listeners();
        }
      }
      goto free_out; /* request queued */
    }
    else if (c_strncmp(data + 4, "/status ", 8) == 0)
    {
      serve_status(http_client);
    }
    else if (c_strncmp(data + 4, "/update?", 8) == 0)
    {
      switch (enduser_setup_http_handle_credentials(data, data_len))
      {
        case 0:
          enduser_setup_http_serve_header(http_client, http_header_302, LITLEN(http_header_302));
          break;
        case 1:
          enduser_setup_http_serve_header(http_client, http_header_401, LITLEN(http_header_401));
          break;
        default:
          ENDUSER_SETUP_ERROR("http_recvcb failed. Failed to handle wifi credentials.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_NONFATAL);
          break;
      }
    }
    else if (c_strncmp(data + 4, "/generate_204 ", 14) == 0)
    {
      /* Convince Android devices that they have internet access to avoid pesky dialogues. */
      enduser_setup_http_serve_header(http_client, http_header_204, LITLEN(http_header_204));
    }
    else
    {
      ENDUSER_SETUP_DEBUG("serving 404");
      ENDUSER_SETUP_DEBUG(data + 4);
      enduser_setup_http_serve_header(http_client, http_header_404, LITLEN(http_header_404));
    }
  }
  else /* not GET */
  {
    enduser_setup_http_serve_header(http_client, http_header_401, LITLEN(http_header_401));
  }

  deferred_close (http_client);

free_out:
  os_free (data);
  return ret;
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
    ENDUSER_SETUP_DEBUG("enduser_setup_http_start info. Loaded backup HTML.");
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
  ENDUSER_SETUP_DEBUG("enduser_setup_station_stop");

  wifi_set_opmode(~SOFTAP_MODE & wifi_get_opmode());
}


static void enduser_setup_ap_start(void)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_ap_start");

  struct softap_config cnf;
  c_memset(&(cnf), 0, sizeof(struct softap_config));

#ifndef ENDUSER_SETUP_AP_SSID
  #define ENDUSER_SETUP_AP_SSID "SetupGadget"
#endif

  char ssid[] = ENDUSER_SETUP_AP_SSID;
  int ssid_name_len = c_strlen(ssid);
  c_memcpy(&(cnf.ssid), ssid, ssid_name_len);

  uint8_t mac[6];
  wifi_get_macaddr(SOFTAP_IF, mac);
  cnf.ssid[ssid_name_len] = '_';
  c_sprintf(cnf.ssid + ssid_name_len + 1, "%02X%02X%02X", mac[3], mac[4], mac[5]);
  cnf.ssid_len = ssid_name_len + 7;
  cnf.channel = 1;
  cnf.authmode = AUTH_OPEN;
  cnf.ssid_hidden = 0;
  cnf.max_connection = 5;
  cnf.beacon_interval = 100;
  wifi_set_opmode(STATIONAP_MODE);
  wifi_softap_set_config(&cnf);
}


static void enduser_setup_dns_recv_callback(void *arg, char *recv_data, unsigned short recv_len)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_dns_recv_callback.");

  struct espconn *callback_espconn = arg;
  struct ip_info ip_info;

  uint32_t qname_len = c_strlen(&(recv_data[12])) + 1; // \0=1byte
  uint32_t dns_reply_static_len = (uint32_t) sizeof(dns_header) + (uint32_t) sizeof(dns_body) + 2 + 4; // dns_id=2bytes, ip=4bytes
  uint32_t dns_reply_len = dns_reply_static_len + qname_len;

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


  char *dns_reply = (char *) c_malloc(dns_reply_len);
  if (dns_reply == NULL)
  {
    ENDUSER_SETUP_ERROR_VOID("dns_recv_callback failed. Failed to allocate memory.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_NONFATAL);
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
    ENDUSER_SETUP_ERROR_VOID("dns_recv_callback failed. Unable to get IP of UDP sender.", ENDUSER_SETUP_ERR_CONNECTION_NOT_FOUND, ENDUSER_SETUP_ERR_FATAL);
  }
  callback_espconn->proto.udp->remote_port = pr->remote_port;
  os_memmove(callback_espconn->proto.udp->remote_ip, pr->remote_ip, 4);

  int8_t err;
  err = espconn_send(callback_espconn, dns_reply, dns_reply_len);
  c_free(dns_reply);
  if (err == ESPCONN_MEM)
  {
    ENDUSER_SETUP_ERROR_VOID("dns_recv_callback failed. Failed to allocate memory for send.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }
  else if (err == ESPCONN_ARG)
  {
    ENDUSER_SETUP_ERROR_VOID("dns_recv_callback failed. Can't execute transmission.", ENDUSER_SETUP_ERR_CONNECTION_NOT_FOUND, ENDUSER_SETUP_ERR_FATAL);
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
      c_free(state->espconn_dns_udp->proto.udp);
    }
    c_free(state->espconn_dns_udp);
  }

  c_free(state->http_payload_data);

  free_scan_listeners ();

  c_free(state);
  state = NULL;
}


static int enduser_setup_dns_start(void)
{
  ENDUSER_SETUP_DEBUG("enduser_setup_dns_start");

  if (state->espconn_dns_udp != NULL)
  {
    ENDUSER_SETUP_ERROR("dns_start failed. Appears to already be started (espconn_dns_udp != NULL).", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
  }
  state->espconn_dns_udp = (struct espconn *) c_malloc(sizeof(struct espconn));
  if (state->espconn_dns_udp == NULL)
  {
    ENDUSER_SETUP_ERROR("dns_start failed. Memory allocation failed (espconn_dns_udp == NULL).", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
  }

  esp_udp *esp_udp_data = (esp_udp *) c_malloc(sizeof(esp_udp));
  if (esp_udp_data == NULL)
  {
    ENDUSER_SETUP_ERROR("dns_start failed. Memory allocation failed (esp_udp == NULL).", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
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
  ENDUSER_SETUP_DEBUG("enduser_setup_init");

  if (state != NULL)
  {
    ENDUSER_SETUP_ERROR("init failed. Appears to already be started.", ENDUSER_SETUP_ERR_UNKOWN_ERROR, ENDUSER_SETUP_ERR_FATAL);
    return ENDUSER_SETUP_ERR_UNKOWN_ERROR;
  }

  state = (enduser_setup_state_t *) os_zalloc(sizeof(enduser_setup_state_t));
  if (state == NULL)
  {
    ENDUSER_SETUP_ERROR("init failed. Unable to allocate memory.", ENDUSER_SETUP_ERR_OUT_OF_MEMORY, ENDUSER_SETUP_ERR_FATAL);
    return ENDUSER_SETUP_ERR_OUT_OF_MEMORY;
  }
  c_memset(state, 0, sizeof(enduser_setup_state_t));

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
  ENDUSER_SETUP_DEBUG("enduser_setup_start");

  if (!do_station_cfg_handle)
  {
    do_station_cfg_handle = task_get_id(do_station_cfg);
  }

  if(enduser_setup_init(L))
  {
    goto failed;
  }

  enduser_setup_check_station_start();
  if (!manual)
  {
    enduser_setup_ap_start();
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
  enduser_setup_dns_stop();
  enduser_setup_http_stop();
  enduser_setup_free();

  return 0;
}


static const LUA_REG_TYPE enduser_setup_map[] = {
  { LSTRKEY( "manual" ), LFUNCVAL( enduser_setup_manual )},
  { LSTRKEY( "start" ), LFUNCVAL( enduser_setup_start )},
  { LSTRKEY( "stop" ),  LFUNCVAL( enduser_setup_stop  )},
  { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(ENDUSER_SETUP, "enduser_setup", enduser_setup_map, NULL);
