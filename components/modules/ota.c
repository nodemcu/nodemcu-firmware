// Module for doing over-the-air (OTA) firmware upgrades

#include <stdarg.h>

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "lwip/dns.h"
#include "esp_partition.h"

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

static int ota_cancel( lua_State *L )
{
  if(!ota_started) {
    OTA_ERROR(L, "Error: OTA not started");
  }

  if (esp_ota_end(ota_update_handle) != ESP_OK) {
    OTA_ERROR(L, "esp_ota_end");
  }

  ota_started = false;

  lua_pushboolean (L, true);
  return 1;
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

/* OTA partition part*/

#define OTA_PARTITION_INFO(L, partition) \
  lua_newtable(L); \
  \
  lua_pushinteger(L, partition->address); \
  lua_setfield(L, -2, "address"); \
  \
  lua_pushinteger(L, partition->size); \
  lua_setfield(L, -2, "size"); \
  \
  lua_pushstring(L, partition->label); \
  lua_setfield(L, -2, "label"); \
  \
  lua_pushboolean(L, partition->encrypted); \
  lua_setfield(L, -2, "encrypted");

static int ota_partition_find( lua_State *L ) {
  int stack = 0;

  size_t label_length;
  const char *label_filter = luaL_optlstring(L, ++stack, NULL, &label_length);

  const esp_partition_t * partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label_filter);
  if(!partition)
    return 0;

  OTA_PARTITION_INFO(L, partition);

  return 1;
}

static int ota_partition_find_all( lua_State *L ) {
  esp_partition_iterator_t iterator = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
  lua_newtable(L);

  int i=0;
  while(iterator) {
    const esp_partition_t * partition = esp_partition_get(iterator);

    lua_pushnumber(L, ++i);

    OTA_PARTITION_INFO(L, partition);
    lua_settable(L, -3);

    iterator = esp_partition_next(iterator);
  }

  esp_partition_iterator_release(iterator);

  return 1;
}

static int ota_partition_get_running_partition( lua_State *L ) {
  const esp_partition_t *partition = esp_ota_get_running_partition();

  OTA_PARTITION_INFO(L, partition);

  return 1;
}

static int ota_partition_get_boot_partition( lua_State *L ) {
  const esp_partition_t *partition = esp_ota_get_boot_partition();

  OTA_PARTITION_INFO(L, partition);

  return 1;
}

static int ota_partition_set_boot( lua_State *L ) {
  int stack = 0;

  const char *label_filter = luaL_checkstring(L, ++stack);

  const esp_partition_t * partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label_filter);
  if(!partition) {
    OTA_ERROR(L, "set_boot failed! partition '%s' not found", label_filter);
  }

  esp_err_t err = esp_ota_set_boot_partition(partition);
  if (err != ESP_OK) {
    OTA_ERROR(L, "esp_ota_set_boot_partition failed! err=0x%x", err);
  }

  lua_pushboolean (L, true);
  return 1;
}

/* Maps */

static const LUA_REG_TYPE ota_partition_map[] = {
  { LSTRKEY( "find_all" ),               LFUNCVAL( ota_partition_find_all ) },
  { LSTRKEY( "find"),                    LFUNCVAL( ota_partition_find ) },

  { LSTRKEY( "get_running"),             LFUNCVAL( ota_partition_get_running_partition ) },
  { LSTRKEY( "get_boot"),                LFUNCVAL( ota_partition_get_boot_partition ) },
  { LSTRKEY( "set_boot"),                LFUNCVAL( ota_partition_set_boot ) },

  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE ota_map[] =
{
  { LSTRKEY( "start" ),       LFUNCVAL( ota_start ) },
  { LSTRKEY( "write" ),       LFUNCVAL( ota_write ) },
  { LSTRKEY( "cancel" ),      LFUNCVAL( ota_cancel ) },
  { LSTRKEY( "complete" ),    LFUNCVAL( ota_complete ) },

  { LSTRKEY( "partition" ),        LROVAL( ota_partition_map ) },

  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(OTA, "ota", ota_map, NULL);
