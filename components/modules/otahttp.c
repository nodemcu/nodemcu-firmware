// Module for downloading over-the-air (OTA) firmware upgrades via http

#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdarg.h>

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "lwip/dns.h"

#include "ota_common.h"

#define BUFFSIZE 1024
#define TEXT_BUFFSIZE 1024

static const char *TAG = "otahttp";

#define OTAHTTP_ERROR(L, format,...) \
  ESP_LOGE(TAG, format, ##__VA_ARGS__); \
  lua_pushboolean (L, false); \
  return 1;

typedef struct {
  bool completed;
  ip_addr_t ip_address;
} otahttp_ip_lookup;

static void dns_lookup_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    otahttp_ip_lookup* lookup = callback_arg;
    lookup->ip_address = *ipaddr;
    lookup->completed = true;
}

static ip_addr_t dns_lookup(const char *domain) {
  otahttp_ip_lookup lookup;

  lookup.completed = false;
  IP_ADDR4( &(lookup.ip_address), 0,0,0,0 );

  err_t  err = dns_gethostbyname(domain, &(lookup.ip_address), dns_lookup_callback, &lookup);
  if(err == ERR_OK)
    dns_lookup_callback(domain, &(lookup.ip_address), &lookup);
  else if (err == ERR_INPROGRESS)
    while(!lookup.completed);

  return lookup.ip_address;
}

static bool connect_to_http_server(const char* domain, uint16_t port)
{
    ESP_LOGI(TAG, "Server: %s Server Port:%d", domain, port);

    ip_addr_t ip_address = dns_lookup(domain);

    if(ip_address.u_addr.ip4.addr == 0) {
      ESP_LOGE(TAG, "Failed to resolve IP address");
      return -2;
    } else {
      ESP_LOGI(TAG, "DNS found: %i.%i.%i.%i\n", ip4_addr1(&ip_address.u_addr.ip4), ip4_addr2(&ip_address.u_addr.ip4), ip4_addr3(&ip_address.u_addr.ip4), ip4_addr4(&ip_address.u_addr.ip4) );
    }

    int http_connect_flag = -1;
    int socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_id == -1) {
        ESP_LOGE(TAG, "Create socket failed!");
        return -1;
    }

    struct sockaddr_in sock_info;
    memset(&sock_info, 0, sizeof(struct sockaddr_in));
    sock_info.sin_family = AF_INET;
    sock_info.sin_addr.s_addr = ip_address.u_addr.ip4.addr;
    sock_info.sin_port = htons(port);

    // connect to http server
    http_connect_flag = connect(socket_id, (struct sockaddr *)&sock_info, sizeof(sock_info));
    if (http_connect_flag == -1) {
        ESP_LOGE(TAG, "Connect to server failed! errno=%d", errno);
        close(socket_id);
        return -3;
    } else {
        ESP_LOGI(TAG, "Connected to server");
        return socket_id;
    }
}

static int find_body_offset(const char *http_response, int total_len)
{
    int i = 0;

    while (http_response[i] != 0 && i + 3 < total_len) {
      if(http_response[i++] == '\r' && http_response[i++] == '\n' && http_response[i++] == '\r' && http_response[i++] == '\n') {
        return i;
      }
    }

    return -1;
}

static int otahttp_download( lua_State *L )
{
  if(!ota_started) {
    OTAHTTP_ERROR(L, "Error: OTA not started");
  }

  int image_length = 0;
  char http_response[BUFFSIZE + 1];
  char write_data[BUFFSIZE + 1];

  esp_err_t err;
  int stack = 0;

  size_t domain_length = 0;
  const char* domain = luaL_checklstring(L, ++stack, &domain_length);
  uint16_t port = luaL_checkinteger(L, ++stack);
  size_t path_length = 0;
  const char* path = luaL_checklstring(L, ++stack, &path_length);

  /*connect to http server*/
  int socket_id = connect_to_http_server(domain, port);
  if (socket_id < 0) {
    OTAHTTP_ERROR(L, "Connect to http server failed! errno=%d", socket_id);
  } else {
    ESP_LOGI(TAG, "Connected to http server");
  }

  /*send GET request to http server*/
  char http_request[64] = {0};
  sprintf(http_request, "GET %s HTTP/1.1\r\nHost: %s:%d \r\n\r\n", path, domain, port);

  int res = send(socket_id, http_request, strlen(http_request), 0);
  if (res == -1) {
    OTAHTTP_ERROR(L, "Send GET request to server failed");
  } else {
    ESP_LOGI(TAG, "Send GET request to server succeeded");
  }

  bool found_body = false, flag = true;
  int packet_counter = 0;

  /* Deal with all receive packets */
  while (flag) {
    memset(&http_response, 0, TEXT_BUFFSIZE);
    memset(&write_data, 0, BUFFSIZE);

    int packet_length = recv(socket_id, &http_response, TEXT_BUFFSIZE, 0);
    if (packet_length < 0) { /* Receive error */
      OTAHTTP_ERROR(L, "Error: receive data error! errno=%d", packet_length);
    } else if(packet_length == 0) { /* No more data, we´re done */
      flag = false;
      ESP_LOGI(TAG, "Connection closed, all packets received");
      close(socket_id);
      break;
    }

    packet_counter++;

    int body_offset = 0;
    if(!found_body) {
      body_offset = find_body_offset(http_response, packet_length);
      if(body_offset == -1)
        continue;

      found_body = true;
    }
    int length = packet_length - body_offset;

    memcpy(&write_data, &(http_response[body_offset]), length);
    err = esp_ota_write( ota_update_handle, (const void *)&write_data, length);
    if (err != ESP_OK) {
      close(socket_id);
      OTAHTTP_ERROR(L, "Error: esp_ota_write failed! err=0x%x", err);
    }

    image_length += length;
    if(packet_counter % 100 == 0) {
      ESP_LOGI(TAG, "Have written image length %d", image_length);
    }
  }

  if(!found_body) {
    OTAHTTP_ERROR(L, "Error: did not found body!");
  } else {
    ESP_LOGI(TAG, "Total Write binary data length : %d", image_length);

    lua_pushboolean (L, true);
    return 1;
  }
}

static const LUA_REG_TYPE otahttp_map[] =
{
  { LSTRKEY( "download" ),      LFUNCVAL( otahttp_download ) },

  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(OTAHTTP, "otahttp", otahttp_map, NULL);
