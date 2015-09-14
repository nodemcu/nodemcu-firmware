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

#define PRINT_FUNC_ENABLE 0
#if PRINT_FUNC_ENABLE
#define PRINT_FUNC c_printf
#else
#define PRINT_FUNC
#endif


/**
 * DNS Response Packet:
 *
 * |DNS ID - 16 bits|
 * |dns_header|
 * |QNAME|
 * |dns_body|
 * |ip - 32 bits|
 *
 *        DNS Header Part                         |  FLAGS | | Q COUNT |  | A CNT  |  |AUTH CNT|  | ADD CNT| */
static const char ROM_CONST_ATTR dns_header[] = { 0x80, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 };
/*        DNS Query Part                          | Q TYPE |  | Q CLASS| */
static const char ROM_CONST_ATTR dns_body[]   = { 0x00, 0x01, 0x00, 0x01,
/*        DNS Answer Part                         |LBL OFFS|  |  TYPE  |  |  CLASS |  |         TTL        |  | RD LEN | */
                                                  0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x04 };

static const char ROM_CONST_ATTR http_html_filename[] = "index.html";
static const char ROM_CONST_ATTR http_header_200[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
static const char ROM_CONST_ATTR http_header_404[] = "HTTP/1.1 404 Not Found\r\n";

static const char ROM_CONST_ATTR http_html_backup[] = "<!DOCTYPE html><html><head><meta charset=utf-8><meta name=viewport content='width=380'><title>Connect gadget to you WiFi</title><style media=screen type=text/css>*{margin:0;padding:0}html{height:100%;background:linear-gradient(rgba(196,102,0,.2),rgba(155,89,182,.2)),url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEYAAAA8AgMAAACm+SSwAAAADFBMVEVBR1FFS1VHTlg8Q0zU/YXIAAADVElEQVQ4yy1TTYvTUBQ9GTKiYNoodsCF4MK6U4TZChOhiguFWHyBFzqlLl4hoeNvEBeCrlrhBVKq1EUKLTP+hvi1GyguXqBdiZCBzGqg20K8L3hDQnK55+OeJNguHx6UujYl3dL5ALn4JOIUluAqeAWciyGaSdvngOWzNT+G0UyGUOxVOAdqkjXDCbBiUyjZ5QzYEbGadYAi6kHxth+kthXNVNCDofwhGv1D4QGGiM9iAjbCHgr2iUUpDJbs+VPQ4xAr2fX7KXbkOJMdok965Ksb+6lrjdkem8AshIuHm9Nyu19uTunYlOXDTQqi8VgeH0kBXH2xq/ouiMZPzuMukymutrBmulUTovC6HqNFW2ZOiqlpSXZOTvSUeUPxChjxol8BLbRy4gJuhV7OR4LRVBs3WQ9VVAU7SXgK2HeUrOj7bC8YsUgr3lEV/TXB7hK90EBnxaeg1Ov15bY80M736ekCGesGAaGvG0Ct4WRkVQVHIgIM9xJgvSFfPay8Q6GNv7VpR7xUnkvhnMQCJDYkYOtNLihV70tCU1Sk+BQrpoP+HLHUrJkuta40C6LP5GvBv+Hqo10ATxxFrTPvNdPr7XwgQud6RvQN/sXjBGzqbU27wcj9cgsyvSTrpyXV8gKpXeNJU3aFl7MOdldzV4+HfO19jBa5f2IjWwx1OLHIvFHkqbBj20ro1g7nDfY1DpScvDRUNARgjMMVO0zoMjKxJ6uWCPP+YRAWbGoaN8kXYHmLjB9FXLGOazfFVCvOgqzfnicNPrHtPKlex2ye824gMza0cTZ2sS2Xm7Qst/UfFw8O6vVtmUKxZy9xFgzMys5cJ5fxZw4y37Ufk1Dsfb8MqOjYxE3ZMWxiDcO0PYUaD2ys+8OW1pbB7/e3sfZeGVCL0Q2aMjjPdm2sxADuejZxHJAd8dO9DSUdA0V8/NggRRanDkBrANn8yHlEQOn/MmwoQfQF7xgmKDnv520bS/pgylP67vf3y2V5sCwfoCEMkZClgOfJAFX9eXefR2RpnmRs4CDVPceaRfoFzCkJVJX27vWZnoqyvmtXU3+dW1EIXIu8Qg5Qta4Zlv7drUCoWe8/8MXzaEwux7ESE9h6qnHj3mIO0/D9RvzfxPmjWiQ1vbeSk4rrHwhAre35EEVaAAAAAElFTkSuQmCC)}body{font-family:arial,verdana}div{position:absolute;margin:auto;top:0;right:0;bottom:0;left:0;width:320px;height:274px}form{width:320px;text-align:center;position:relative}form fieldset{background:#fff;border:0 none;border-radius:5px;box-shadow:0 0 15px 1px rgba(0,0,0,.4);padding:20px 30px;box-sizing:border-box}form input{padding:15px;border:1px solid #ccc;border-radius:3px;margin-bottom:10px;width:100%;box-sizing:border-box;font-family:montserrat;color:#2C3E50;font-size:13px}form .action-button{width:100px;background:#27AE60;font-weight:700;color:#fff;border:0 none;border-radius:3px;cursor:pointer;padding:10px 5px;margin:10px 5px}#msform .action-button:focus,form .action-button:hover{box-shadow:0 0 0 2px #fff,0 0 0 3px #27AE60}.fs-title{font-size:15px;text-transform:uppercase;color:#2C3E50;margin-bottom:10px}.fs-subtitle{font-weight:400;font-size:13px;color:#666;margin-bottom:20px}</style><body><div><form><fieldset><h2 class=fs-title>WiFi Login</h2><h3 class=fs-subtitle>Connect gadget to your WiFi</h3><input autocorrect=off autocapitalize=none name=wifi_ssid placeholder='WiFi Name'> <input type=password name=wifi_password placeholder='Password'1> <input type=submit name=save class='submit action-button' value='Save'></fieldset></form></div>";

static struct espconn *espconn_dns_udp;
static struct espconn *espconn_http_tcp;
typedef struct http_payload_t {
  char *data;
  uint32_t len;
} http_payload_t;
static http_payload_t http_payload;
static os_timer_t check_station_timer;

static int enduser_setup_start(lua_State* L);
static int enduser_setup_stop(lua_State* L);
static void enduser_setup_station_start(void);
static void enduser_setup_ap_start(void);
static void enduser_setup_ap_stop(void);
static void enduser_setup_check_station(void);


static void enduser_setup_check_station_start(void)
{
  PRINT_FUNC("enduser_setup_check_station_start\n");

  os_timer_setfn(&check_station_timer, enduser_setup_check_station, NULL);
  os_timer_arm(&check_station_timer, 1*1000, TRUE);
}


static void enduser_setup_check_station_stop(void)
{
  PRINT_FUNC("enduser_setup_check_station_stop\n");

  os_timer_disarm(&check_station_timer);
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

  uint8_t *ip_byte = (uint8_t *) &(ip.ip);
  NODE_DEBUG("Connected to \"%s\" as %u.%u.%u.%u\n", cnf.ssid, IP2STR(&ip));
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


static void enduser_setup_http_free(void)
{
  http_payload.len = 0;
  c_free(http_payload.data);
  http_payload.data = NULL;
  
  if (espconn_http_tcp != NULL)
  {
    if (espconn_http_tcp->proto.tcp != NULL)
    {
      c_free(espconn_http_tcp->proto.tcp);
    }
    c_free(espconn_http_tcp);
    espconn_http_tcp = NULL;
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
  PRINT_FUNC("enduser_setup_http_load_payload\n");

  int f = fs_open(http_html_filename, fs_mode2flag("r"));
  int err = fs_seek(f, 0, FS_SEEK_END);
  int file_len = (int) fs_tell(f);
  int err2 = fs_seek(f, 0, FS_SEEK_SET);
  
  if (f == 0 || err == -1 || err2 == -1)
  {
    NODE_DEBUG("enduser_setup_http_load_payload unable to load file \"%s\", loading backup HTML.\n", http_html_filename);

    int payload_len = sizeof(http_header_200) + sizeof(http_html_backup);
    http_payload.len = payload_len;
    http_payload.data = (char *) c_malloc(payload_len);
    if (http_payload.data == NULL)
    {
      return 2;
    }
    
    int offset = 0;
    c_memcpy(&(http_payload.data[offset]), &(http_header_200), sizeof(http_header_200));
    offset += sizeof(http_header_200);
    c_memcpy(&(http_payload.data[offset]), &(http_html_backup), sizeof(http_html_backup));

    return 1;
  }
  
  int payload_len = sizeof(http_header_200) + file_len;
  http_payload.len = payload_len;
  http_payload.data = (char *) c_malloc(payload_len);
  if (http_payload.data == NULL)
  {
    return 2;
  }
  
  int offset = 0;
  c_memcpy(&(http_payload.data[offset]), &(http_header_200), sizeof(http_header_200));
  offset += sizeof(http_header_200);
  fs_read(f, &(http_payload.data[offset]), file_len);
  
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
  PRINT_FUNC("enduser_setup_http_handle_credentials\n");

  char *name_str = (char *) ((uint32_t)strstr(&(data[5]), "?wifi_ssid=") + (uint32_t)strstr(&(data[5]), "&wifi_ssid="));
  char *pwd_str = (char *) ((uint32_t)strstr(&(data[5]), "?wifi_password=") + (uint32_t)strstr(&(data[5]), "&wifi_password="));
  if (name_str == NULL || pwd_str == NULL)
  {
    return 1;
  }

  int name_field_len = sizeof("?wifi_ssid=") - 1;
  int pwd_field_len = sizeof("?wifi_password=") - 1;
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
    NODE_DEBUG("enduser_setup_station_start failed. wifi_set_opmode failed.\n");
    wifi_set_opmode(~STATION_MODE & wifi_get_opmode());
    return 2;
  }
  err = wifi_station_set_config(&cnf);
  if (err == FALSE)
  {
    NODE_DEBUG("enduser_setup_station_start failed. wifi_station_set_config failed.\n");
    wifi_set_opmode(~STATION_MODE & wifi_get_opmode());
    return 2;
  }
  err = wifi_station_disconnect();
  if (err == FALSE)
  {
    NODE_DEBUG("enduser_setup_station_start failed. wifi_station_disconnect failed.\n");
  }
  err = wifi_station_connect();
  if (err == FALSE)
  {
    NODE_DEBUG("enduser_setup_station_start failed. wifi_station_connect failed.\n");
  }

  NODE_DEBUG("\n");  
  NODE_DEBUG("WiFi Credentials Stored\n");
  NODE_DEBUG("-----------------------\n");
  NODE_DEBUG("name: \"%s\"\n", cnf.ssid);
  NODE_DEBUG("pass: \"%s\"\n", cnf.password);
  NODE_DEBUG("bssid_set: %u\n", cnf.bssid_set);
  NODE_DEBUG("bssid: \"%s\"\n", cnf.bssid);
  NODE_DEBUG("-----------------------\n\n");

  return 0;
}


/**
 * Serve HTML
 * 
 * @return - return 0 iff html was served successfully
 */
static int enduser_setup_http_serve_header(struct espconn *http_client, char *header, uint32_t header_len)
{
  PRINT_FUNC("enduser_setup_http_serve_404\n");
  
  int8_t err = espconn_sent(http_client, header, header_len);
  if (err == ESPCONN_MEM)
  {
    NODE_DEBUG("enduser_setup_http_serve_header failed. espconn_send out of memory\n");
    return 1;
  }
  else if (err == ESPCONN_ARG)
  {
    NODE_DEBUG("enduser_setup_http_serve_header failed. espconn_send can't find network transmission\n");
    return 1;
  }
  else if (err != 0)
  {
    NODE_DEBUG("enduser_setup_http_serve_header failed. espconn_send failed\n");
    return 1;  
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
  PRINT_FUNC("enduser_setup_http_serve_html\n");
  
  if (http_payload.data == NULL)
  {
    enduser_setup_http_load_payload();
  }
  
  int8_t err = espconn_sent(http_client, http_payload.data, http_payload.len);
  if (err == ESPCONN_MEM)
  {
    NODE_DEBUG("enduser_setup_http_serve_html failed. espconn_send out of memory\n");
    return 1;
  }
  else if (err == ESPCONN_ARG)
  {
    NODE_DEBUG("enduser_setup_http_serve_html failed. espconn_send can't find network transmission\n");
    return 1;
  }
  else if (err != 0)
  {
    NODE_DEBUG("enduser_setup_http_serve_html failed. espconn_send failed\n");
    return 1;  
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
  PRINT_FUNC("enduser_setup_http_disconnect\n");
//TODO: Construct and maintain os task queue(?) to be able to issue system_os_task with espconn_disconnect.
}


static void enduser_setup_http_recvcb(void *arg, char *data, unsigned short data_len)
{
  PRINT_FUNC("enduser_setup_http_recvcb\n");
  struct espconn *http_client = (struct espconn *) arg;

  if (c_strncmp(data, "GET ", 4) != 0)
  {
    enduser_setup_http_serve_header(http_client, (char *) http_header_404, sizeof(http_header_404));
    enduser_setup_http_disconnect(http_client);
    return;
  }
  
  int retval = enduser_setup_http_handle_credentials(data, data_len);
  if (retval == 0)
  {
    enduser_setup_http_serve_header(http_client, (char *) http_header_200, sizeof(http_header_200));
    enduser_setup_http_disconnect(http_client);
    return;
  }
  else if (retval == 2)
  {
    NODE_DEBUG("enduser_setup_http_recvcb failed. Failed to handle wifi credentials.\n");
    return;
  }
  
  if (retval != 1)
  {
    NODE_DEBUG("enduser_setup_http_recvcb failed. Unknown error code #%u.\n", retval);
    return;
  }
  
  /* Reject requests that probably aren't relevant to free up resources. */
  if (c_strncmp(data, "GET / ", 6) != 0)
  {
    PRINT_DEBUG("enduser_setup_http_recvcb received too specific request.\n");
    enduser_setup_http_serve_header(http_client, (char *) http_header_404, sizeof(http_header_404));
    enduser_setup_http_disconnect(http_client);
    return;
  }
 
  retval = enduser_setup_http_serve_html(http_client);
  if (retval != 0)
  {
    NODE_DEBUG("enduser_setup_http_recvcb failed. Unable to send HTML.\n");
    enduser_setup_http_disconnect(http_client);
  }
}


static void enduser_setup_http_connectcb(void *arg)
{
  PRINT_FUNC("enduser_setup_http_connectcb\n");

  struct espconn *callback_espconn = (struct espconn *) arg;

  int8_t err = 0;
  err |= espconn_regist_recvcb(callback_espconn, enduser_setup_http_recvcb);
  
  if (err != 0)
  {
    NODE_DEBUG("enduser_setup_http_connectcb failed. Callback registration failed.\n");
  }
}


static void enduser_setup_http_start(void)
{
  PRINT_FUNC("enduser_setup_http_start\n");

  if (espconn_http_tcp != NULL)
  {
    NODE_DEBUG("enduser_setup_http_start failed. Appears to already be started (espconn_http_tcp != NULL).\n");
    return;
  }
  espconn_http_tcp = (struct espconn *) c_malloc(sizeof(struct espconn));
  if (espconn_http_tcp == NULL)
  {
    NODE_DEBUG("enduser_setup_http_start failed. Memory allocation failed (espconn_http_tcp == NULL).\n");
    return;
  }

  esp_tcp *esp_tcp_data = (esp_tcp *) c_malloc(sizeof(esp_tcp));
  if (esp_tcp_data == NULL)
  {
    NODE_DEBUG("enduser_setup_http_start failed. Memory allocation failed (esp_udp == NULL).\n");
    enduser_setup_http_free();
    return;
  }

  c_memset(espconn_http_tcp, 0, sizeof(struct espconn));
  c_memset(esp_tcp_data, 0, sizeof(esp_tcp));
  espconn_http_tcp->proto.tcp = esp_tcp_data;
  espconn_http_tcp->type = ESPCONN_TCP;
  espconn_http_tcp->state = ESPCONN_NONE;
  esp_tcp_data->local_port = 80;

  int8_t err;
  err = espconn_regist_connectcb(espconn_http_tcp, enduser_setup_http_connectcb);
  if (err != 0)
  {
    NODE_DEBUG("enduser_setup_http_start failed. Couldn't add receive callback, ERRROR #%u.\n", err);
    enduser_setup_http_free();
    return;
  }

  err = espconn_accept(espconn_http_tcp);
  if (err == ESPCONN_ISCONN)
  {
    NODE_DEBUG("enduser_setup_http_start failed. Couldn't create connection, already listening for that connection.\n");
    enduser_setup_http_free();
    return;
  }
  else if (err == ESPCONN_MEM)
  {
    NODE_DEBUG("enduser_setup_http_start failed. Couldn't create connection, out of memory.\n");
    enduser_setup_http_free();
    return;
  }
  else if (err == ESPCONN_ARG)
  {
    NODE_DEBUG("enduser_setup_http_start failed. Can't find connection from espconn argument\n");
    enduser_setup_http_free();
    return;  
  }
  else if (err != 0)
  {
    NODE_DEBUG("enduser_setup_http_start failed. ERRROR #%u\n", err);
    enduser_setup_http_free();
    return;  
  }
  
  err = espconn_regist_time(espconn_http_tcp, 2, 0);
  if (err == ESPCONN_ARG)
  {
    NODE_DEBUG("enduser_setup_http_start failed. Unable to set TCP timeout.\n");
    enduser_setup_http_free();
    return;  
  }

  err = enduser_setup_http_load_payload();
  if (err == 1)
  {
    PRINT_DEBUG("enduser_setup_http_start info. Loaded backup HTML.\n");
  }
  else if (err == 2)
  {
    NODE_DEBUG("enduser_setup_http_start failed. Unable to allocate memory for HTTP payload.\n");
    enduser_setup_http_free();
    return;  
  }
}


static void enduser_setup_http_stop(void)
{
  PRINT_FUNC("enduser_setup_http_stop\n");

  if (espconn_http_tcp == NULL)
  {
    NODE_DEBUG("enduser_setup_http_stop failed. enduser_setup not enabled (espconn_dns_udp == NULL).\n");
    return;
  }

  int8_t err = espconn_delete(espconn_http_tcp);
  if (err == ESPCONN_ARG)
  {
    NODE_DEBUG("enduser_setup_http_stop failed. espconn_delete returned ESPCONN_ARG. Can't find network transmission described.\n");
  }
  else if (err != 0)
  {
    NODE_DEBUG("enduser_setup_http_stop failed. espconn_delete returned ERROR #%u.\n", err);
  }

  enduser_setup_http_free();
}

static void enduser_setup_ap_stop(void)
{
  PRINT_FUNC("enduser_setup_station_stop\n");

  wifi_set_opmode(~SOFTAP_MODE & wifi_get_opmode());
}


static void enduser_setup_ap_start(void)
{
  PRINT_FUNC("enduser_setup_ap_start\n");

  struct softap_config cnf;
  char ssid[] = "SetupGadget";
  c_memcpy(&(cnf.ssid), ssid, c_strlen(ssid));
  cnf.ssid_len = c_strlen(ssid);
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
  PRINT_FUNC("enduser_setup_dns_recv_callback received query for %s\n", &(recv_data[12]));

  struct espconn *callback_espconn = arg;
  struct ip_info ip_info;

  uint8_t if_mode = wifi_get_opmode();
  if ((if_mode & SOFTAP_MODE) == 0)
  {
    NODE_DEBUG("enduser_setup_dns_recv_callback failed. Interface mode %d not supported.\n", if_mode);
    return;
  }
  uint8_t if_index = (if_mode == STATION_MODE? STATION_IF : SOFTAP_IF);
  if (wifi_get_ip_info(if_index , &ip_info) == false)
  {
    NODE_DEBUG("enduser_setup_dns_recv_callback failed. Unable to get interface IP.\n");
    return;
  }

  uint32_t qname_len = c_strlen(&(recv_data[12])) + 1; // \0=1byte
  uint32_t dns_reply_static_len = (uint32_t) sizeof(dns_header) + (uint32_t) sizeof(dns_body) + 2 + 4; // dns_id=2bytes, ip=4bytes
  uint32_t dns_reply_len = dns_reply_static_len + qname_len;
  
  char *dns_reply = (char *) c_malloc(dns_reply_len);
  if (dns_reply == NULL)
  {
    NODE_DEBUG("enduser_setup_dns_recv_callback failed. Failed to allocate memory.\n");
    return;
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
  
  int8_t err;
  err = espconn_sent(callback_espconn, dns_reply, dns_reply_len);
  c_free(dns_reply);
  if (err == ESPCONN_MEM)
  {
    NODE_DEBUG("enduser_setup_dns_recv_callback failed. Failed to allocate memory for send.\n");
    return;
  }
  else if (err == ESPCONN_ARG)
  {
    NODE_DEBUG("enduser_setup_dns_recv_callback failed. Can't execute transmission.\n");
    return;  
  }
  else if (err != 0)
  {
    NODE_DEBUG("enduser_setup_dns_recv_callback failed. espconn_send failed\n");
    return;
  }
}


static void enduser_setup_dns_free(void)
{
  PRINT_FUNC("enduser_setup_dns_free\n");

  if (espconn_dns_udp != NULL)
  {
    if (espconn_dns_udp->proto.udp != NULL)
    {
      c_free(espconn_dns_udp->proto.udp);
    }
    c_free(espconn_dns_udp);
    espconn_dns_udp = NULL;
  }
}


static void enduser_setup_dns_start(void)
{
  PRINT_FUNC("enduser_setup_dns_started\n");

  if (espconn_dns_udp != NULL)
  {
    NODE_DEBUG("enduser_setup_dns_start failed. Appears to already be started (espconn_dns_udp != NULL).\n");
    return;
  }
  espconn_dns_udp = (struct espconn *) c_malloc(sizeof(struct espconn));
  if (espconn_dns_udp == NULL)
  {
    NODE_DEBUG("enduser_setup_dns_start failed. Memory allocation failed (espconn_dns_udp == NULL).\n");
    return;
  }

  esp_udp *esp_udp_data = (esp_udp *) c_malloc(sizeof(esp_udp));
  if (esp_udp_data == NULL)
  {
    NODE_DEBUG("enduser_setup_dns_start failed. Memory allocation failed (esp_udp == NULL).\n");
    enduser_setup_dns_free();
    return;
  }

  c_memset(espconn_dns_udp, 0, sizeof(struct espconn));
  c_memset(esp_udp_data, 0, sizeof(esp_udp));
  espconn_dns_udp->proto.udp = esp_udp_data;
  espconn_dns_udp->type = ESPCONN_UDP;
  espconn_dns_udp->state = ESPCONN_NONE;
  esp_udp_data->local_port = 53;

  int8_t err;
  err = espconn_regist_recvcb(espconn_dns_udp, enduser_setup_dns_recv_callback);
  if (err != 0)
  {
    NODE_DEBUG("enduser_setup_dns_start failed. Couldn't add receive callback, ERRROR #%d.\n", err);
    enduser_setup_dns_free();
    return;
  }

  err = espconn_create(espconn_dns_udp);
  if (err == ESPCONN_ISCONN)
  {
    NODE_DEBUG("enduser_setup_dns_start failed. Couldn't create connection, already listening for that connection.\n");
    enduser_setup_dns_free();
    return;
  }
  else if (err == ESPCONN_MEM)
  {
    NODE_DEBUG("enduser_setup_dns_start failed. Couldn't create connection, out of memory.\n");
    enduser_setup_dns_free();
    return;
  }
  else if (err != 0)
  {
    NODE_DEBUG("enduser_setup_dns_start failed. Couldn't create connection, ERROR #%d.\n", err);
    enduser_setup_dns_free();
    return;  
  }
}


static void enduser_setup_dns_stop(void)
{
  PRINT_FUNC("enduser_setup_dns_stop\n");

  if (espconn_dns_udp == NULL)
  {
    NODE_DEBUG("enduser_setup_dns_stop failed. Hijacker not enabled (espconn_dns_udp == NULL).\n");
    return;
  }

  int8_t err = espconn_delete(espconn_dns_udp);
  if (err == ESPCONN_ARG)
  {
    NODE_DEBUG("enduser_setup_http_stop failed. espconn_delete returned ESPCONN_ARG. Can't find network transmission described.\n");
  }
  else if (err != 0)
  {
    NODE_DEBUG("enduser_setup_http_stop failed. espconn_delete returned ERROR #%u.\n", err);
  }

  enduser_setup_dns_free();
}


static int enduser_setup_start(lua_State* L)
{
  enduser_setup_check_station_start();
  enduser_setup_ap_start();
  enduser_setup_dns_start();
  enduser_setup_http_start();
  
  return 0;
}


static int enduser_setup_stop(lua_State* L)
{
  enduser_setup_check_station_stop();
  enduser_setup_ap_stop();
  enduser_setup_dns_stop();
  enduser_setup_http_stop();

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

