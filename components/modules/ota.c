// Module for doing over-the-air (OTA) firmware upgrades

#include <stdarg.h>

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "lwip/dns.h"

#include "ota_common.h"

static const char *TAG = "ota";
const esp_partition_t* ota_update_partition;

#define OTA_ERROR(L, format,...) \
  ESP_LOGE(TAG, format, ##__VA_ARGS__); \
  lua_pushboolean (L, false); \
  return 1;

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

  ota_update_partition = esp_ota_get_next_update_partition(NULL);
  ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x", ota_update_partition->subtype, ota_update_partition->address);
  assert(ota_update_partition != NULL);

  esp_err_t err = esp_ota_begin(ota_update_partition, OTA_SIZE_UNKNOWN, &(ota_update_handle));
  if (err != ESP_OK) {
    ota_started = false;
    OTA_ERROR(L, "esp_ota_begin failed, error=%d", err);
  } else {
    ota_started = true;
    ESP_LOGI(TAG, "esp_ota_begin succeeded");

    lua_pushboolean (L, true);
    return 1;
  }
};

static int ota_write( lua_State *L )
{
  if(!ota_started) {
    OTA_ERROR(L, "Error: OTA not started");
  }

  int stack = 0;

  size_t datalen;
  const char *data = luaL_checklstring(L, ++stack, &datalen);

  esp_err_t err = esp_ota_write(ota_update_handle, data, datalen);
  if (err != ESP_OK) {
    OTA_ERROR(L, "Error: esp_ota_write failed! err=0x%x", err);
  } else {
    lua_pushboolean (L, true);
    return 1;
  }
}

static int ota_complete( lua_State *L )
{
  if(!ota_started) {
    OTA_ERROR(L, "Error: OTA not started");
  }

  if (esp_ota_end(ota_update_handle) != ESP_OK) {
    OTA_ERROR(L, "esp_ota_end");
  }

  esp_err_t err = esp_ota_set_boot_partition(ota_update_partition);
  if (err != ESP_OK) {
    OTA_ERROR(L, "esp_ota_set_boot_partition failed! err=0x%x", err);
  } else {
    ESP_LOGI(TAG, "Prepare to restart system!");
    esp_restart();

    lua_pushboolean (L, true);
    return 1;
  }
}

static const LUA_REG_TYPE ota_map[] =
{
  { LSTRKEY( "start" ),       LFUNCVAL( ota_start ) },
  { LSTRKEY( "write" ),       LFUNCVAL( ota_write ) },
  { LSTRKEY( "complete" ),    LFUNCVAL( ota_complete ) },

  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(OTA, "ota", ota_map, NULL);
