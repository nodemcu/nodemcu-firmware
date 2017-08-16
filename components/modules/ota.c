// Module for doing over-the-air (OTA) firmware upgrades

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

#define BUFFSIZE 1024
#define TEXT_BUFFSIZE 1024

static const char *TAG = "ota";

#define OTA_ERROR(L, format,...) \
  ESP_LOGE(TAG, format, ##__VA_ARGS__); \
  lua_pushboolean (L, false); \
  return 1;

typedef struct {
  esp_ota_handle_t update_handle;
  const esp_partition_t* update_partition;
  bool success;

  int socket_id;
  int image_length;
  char http_response[BUFFSIZE + 1];
  char write_data[BUFFSIZE + 1];
} ota_session;

typedef struct {
  bool completed;
  ip_addr_t ip_address;
} ota_ip_lookup;

static void dns_lookup_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    ota_ip_lookup* lookup = callback_arg;
    lookup->ip_address = *ipaddr;
    lookup->completed = true;
}

static ip_addr_t dns_lookup(const char *domain) {
  ota_ip_lookup lookup;

  lookup.completed = false;
  IP_ADDR4( &(lookup.ip_address), 0,0,0,0 );

  err_t  err = dns_gethostbyname(domain, &(lookup.ip_address), dns_lookup_callback, &lookup);
  if(err == ERR_OK)
    dns_lookup_callback(domain, &(lookup.ip_address), &lookup);
  else if (err == ERR_INPROGRESS)
    while(!lookup.completed);

  return lookup.ip_address;
}

static int read_until(char *buffer, char delim, int len)
{
    int i = 0;
    while (buffer[i] != delim && i < len) {
        ++i;
    }
    return i + 1;
}

static bool connect_to_http_server(ota_session* state, const char* domain, uint16_t port)
{
    ESP_LOGI(TAG, "Server: %s Server Port:%d", domain, port);

    ip_addr_t ip_address = dns_lookup(domain);

    if(ip_address.u_addr.ip4.addr == 0) {
      ESP_LOGE(TAG, "Failed to resolve IP address");
      return false;
    } else {
      ESP_LOGI(TAG, "DNS found: %i.%i.%i.%i\n", ip4_addr1(&ip_address.u_addr.ip4), ip4_addr2(&ip_address.u_addr.ip4), ip4_addr3(&ip_address.u_addr.ip4), ip4_addr4(&ip_address.u_addr.ip4) );
    }

    int  http_connect_flag = -1;
    state->socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (state->socket_id == -1) {
        ESP_LOGE(TAG, "Create socket failed!");
        return false;
    }

    struct sockaddr_in sock_info;
    memset(&sock_info, 0, sizeof(struct sockaddr_in));
    sock_info.sin_family = AF_INET;
    sock_info.sin_addr.s_addr = ip_address.u_addr.ip4.addr;
    sock_info.sin_port = htons(port);

    // connect to http server
    http_connect_flag = connect(state->socket_id, (struct sockaddr *)&sock_info, sizeof(sock_info));
    if (http_connect_flag == -1) {
        ESP_LOGE(TAG, "Connect to server failed! errno=%d", errno);
        close(state->socket_id);
        return false;
    } else {
        ESP_LOGI(TAG, "Connected to server");
        return true;
    }

    return false;
}

static bool read_past_http_header(ota_session* session, int total_len)
{
    /* i means current position */
    int i = 0, i_read_len = 0;

    while (session->http_response[i] != 0 && i < total_len) {
        i_read_len = read_until(&(session->http_response[i]), '\n', total_len);
        // if we resolve \r\n line,we think packet header is finished
        if (i_read_len == 2) {
            int i_write_len = total_len - (i + 2);
            memset(session->write_data, 0, BUFFSIZE);
            /*copy first http packet body to write buffer*/
            memcpy(session->write_data, &(session->http_response[i + 2]), i_write_len);

            esp_err_t err = esp_ota_write( session->update_handle, (const void *)session->write_data, i_write_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
                return false;
            } else {
                ESP_LOGI(TAG, "esp_ota_write header OK");
                session->image_length += i_write_len;
            }
            return true;
        }
        i += i_read_len;
    }

    return false;
}

static int ota_start( lua_State *L )
{
  ESP_LOGI(TAG, "Starting OTA session...");

  const esp_partition_t *configured = esp_ota_get_boot_partition();
  const esp_partition_t *running = esp_ota_get_running_partition();

  if (configured != running) {
     ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x", configured->address, running->address);
     ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
  }

  ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)", running->type, running->subtype, running->address);

  const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
  ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x", update_partition->subtype, update_partition->address);
  assert(update_partition != NULL);

  esp_ota_handle_t update_handle;
  esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &(update_handle));
  if (err != ESP_OK) {
    ESP_LOGE(TAG,  "esp_ota_begin failed, error=%d", err);
  } else {
    ESP_LOGI(TAG, "esp_ota_begin succeeded");

    ota_session* session = lua_newuserdata(L, sizeof(ota_session));
    luaL_getmetatable(L, "ota.session");
    lua_setmetatable(L, -2);

    session->update_partition = update_partition;
    session->update_handle = update_handle;
  }

  return 1;
};

static int ota_download( lua_State *L )
{
  esp_err_t err;
  int stack = 0;
  ota_session* session = (ota_session*)luaL_checkudata(L, ++stack, "ota.session");

  size_t domain_length = 0;
  const char* domain = luaL_checklstring(L, ++stack, &domain_length);
  uint16_t port = luaL_checkinteger(L, ++stack);
  size_t path_length = 0;
  const char* path = luaL_checklstring(L, ++stack, &path_length);

  /*connect to http server*/
  if (connect_to_http_server(session, domain, port)) {
    ESP_LOGI(TAG, "Connected to http server");
  } else {
    OTA_ERROR(L, "Connect to http server failed!");
  }

  int res = -1;
  /*send GET request to http server*/
  char http_request[64] = {0};
  sprintf(http_request, "GET %s HTTP/1.1\r\nHost: %s:%d \r\n\r\n", path, domain, port);
  res = send(session->socket_id, http_request, strlen(http_request), 0);
  if (res == -1) {
    OTA_ERROR(L, "Send GET request to server failed");
  } else {
    ESP_LOGI(TAG, "Send GET request to server succeeded");
  }

  bool resp_body_start = false, flag = true;

  /*deal with all receive packet*/
  while (flag) {
    memset(session->http_response, 0, TEXT_BUFFSIZE);
    memset(session->write_data, 0, BUFFSIZE);
    int buff_len = recv(session->socket_id, session->http_response, TEXT_BUFFSIZE, 0);
    if (buff_len < 0) { /*receive error*/
      OTA_ERROR(L, "Error: receive data error! errno=%d", errno);
    } else if (buff_len > 0 && !resp_body_start) { /*deal with response header*/
        memcpy(session->write_data, session->http_response, buff_len);
        bool success = read_past_http_header(session, buff_len);
        if(success) {
          resp_body_start = true;
        } else {
          OTA_ERROR(L, "read_past_http_header");
        }
    } else if (buff_len > 0 && resp_body_start) { /*deal with response body*/
        memcpy(session->write_data, session->http_response, buff_len);
        err = esp_ota_write( session->update_handle, (const void *)session->write_data, buff_len);
        if (err != ESP_OK) {
          OTA_ERROR(L, "Error: esp_ota_write failed! err=0x%x", err);
        }

        session->image_length += buff_len;
        ESP_LOGI(TAG, "Have written image length %d", session->image_length);
    } else if (buff_len == 0) {  /*packet over*/
        flag = false;
        ESP_LOGI(TAG, "Connection closed, all packets received");
        close(session->socket_id);
    } else {
        ESP_LOGE(TAG, "Unexpected recv result");
    }
  }

  ESP_LOGI(TAG, "Total Write binary data length : %d", session->image_length);

  lua_pushboolean (L, true);
  return 1;
}

static int ota_write( lua_State *L )
{
  esp_err_t err;
  int stack = 0;
  ota_session* session = (ota_session*)luaL_checkudata(L, ++stack, "ota.session");

  size_t datalen;
  const char *data = luaL_checklstring(L, ++stack, &datalen);

  err = esp_ota_write( session->update_handle, data, datalen);
  if (err != ESP_OK) {
    OTA_ERROR(L, "Error: esp_ota_write failed! err=0x%x", err);
  }

  lua_pushboolean (L, true);
  return 1;
}

static int ota_complete( lua_State *L )
{
  esp_err_t err;

  ota_session* session = (ota_session*)luaL_checkudata(L, 1, "ota.session");

  if (esp_ota_end(session->update_handle) != ESP_OK) {
    OTA_ERROR(L, "esp_ota_end");
  }

  err = esp_ota_set_boot_partition(session->update_partition);
  if (err != ESP_OK) {
    OTA_ERROR(L, "esp_ota_set_boot_partition failed! err=0x%x", err);
  } else {
    ESP_LOGI(TAG, "Prepare to restart system!");
    esp_restart();
  }

  lua_pushboolean (L, true);
  return 1;
}

// Module function map
static const LUA_REG_TYPE ota_session_map[] =
{
  { LSTRKEY( "download" ),    LFUNCVAL( ota_download ) },
  { LSTRKEY( "write" ),       LFUNCVAL( ota_write ) },
  { LSTRKEY( "complete" ),    LFUNCVAL( ota_complete ) },

  { LSTRKEY( "__index" ),     LROVAL( ota_session_map )},
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE ota_map[] =
{
  { LSTRKEY( "start" ),      LFUNCVAL( ota_start ) },

  { LNILKEY, LNILVAL }
};

int luaopen_ota(lua_State *L) {
  luaL_rometatable(L, "ota.session", (void *)ota_session_map);  // create metatable for ledc.channel
  return 0;
}

NODEMCU_MODULE(OTA, "ota", ota_map, luaopen_ota);
