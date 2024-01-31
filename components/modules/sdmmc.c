
#include "module.h"
#include "lauxlib.h"

#include "lmem.h"

#include "esp_vfs_fat.h"
#include "diskio_impl.h" // for ff_diskio_get_drive
#include "diskio_sdmmc.h"

#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"

#include "common.h"
#include "platform.h"

// We're limiting ourselves to the number of FAT volumes configured.
#define NUM_CARDS FF_VOLUMES
sdmmc_card_t *lsdmmc_card[NUM_CARDS];

// local definition for SDSPI host
#define LSDMMC_HOST_SDSPI 100
#ifdef CONFIG_IDF_TARGET_ESP32
#define LSDMMC_HOST_HSPI (LSDMMC_HOST_SDSPI + HSPI_HOST)
#define LSDMMC_HOST_VSPI (LSDMMC_HOST_SDSPI + VSPI_HOST)
#endif
#define LSDMMC_HOST_SPI2 (LSDMMC_HOST_SDSPI + SPI2_HOST)
#define LSDMMC_HOST_SPI3 (LSDMMC_HOST_SDSPI + SPI3_HOST)

typedef struct {
  sdmmc_card_t *card;
  FATFS *fs;
  int base_path_ref;
  BYTE pdrv;
} lsdmmc_ud_t;


static bool is_field_present(lua_State *L, int idx, const char *field)
{
  lua_getfield(L, idx, field);
  bool present = !lua_isnoneornil(L, -1);
  lua_pop(L, 1);
  return present;
}


static esp_err_t sdmmc_mount_fat(lsdmmc_ud_t *ud, const char *base_path)
{
  esp_err_t err = ff_diskio_get_drive(&ud->pdrv);
  if (err != ESP_OK)
    return err;

  ff_diskio_register_sdmmc(ud->pdrv, ud->card);

  char drv[3] = { (char)('0' + ud->pdrv), ':', 0 };
  err = esp_vfs_fat_register(
    base_path, drv, CONFIG_NODEMCU_MAX_OPEN_FILES, &ud->fs);
  if (err != ESP_OK)
    goto fail;

  if (f_mount(ud->fs, drv, 1) != FR_OK)
  {
    err = ESP_FAIL;
    goto fail;
  }

  return ESP_OK;

fail:
  if (ud->fs)
  {
    f_mount(NULL, drv, 0);
    ud->fs = NULL;
  }
  esp_vfs_fat_unregister_path(base_path);
  ff_diskio_unregister(ud->pdrv);

  return err;
}


static esp_err_t sdmmc_unmount_fat(lsdmmc_ud_t *ud, const char *base_path)
{
  char drv[3] = { (char)('0' + ud->pdrv), ':', 0 };
  f_mount(NULL, drv, 0);
  ud->fs = NULL;

  esp_err_t err = esp_vfs_fat_unregister_path(base_path);
  ff_diskio_unregister(ud->pdrv);

  return err;
}


static int lsdmmc_init( lua_State *L )
{
  const char *err_msg = "";
  int stack = 0;
  int top = lua_gettop( L );

  int slot = luaL_checkint( L, ++stack );
  luaL_argcheck( L, slot == SDMMC_HOST_SLOT_0 || slot == SDMMC_HOST_SLOT_1 ||
#ifdef CONFIG_IDF_TARGET_ESP32
                    slot == LSDMMC_HOST_HSPI || slot == LSDMMC_HOST_VSPI ||
#endif
                    slot == LSDMMC_HOST_SPI2 || slot == LSDMMC_HOST_SPI3,
                 stack, "invalid slot" );

  bool is_sdspi = false;
  if (slot >= LSDMMC_HOST_SDSPI) {
    is_sdspi = true;
    slot -= LSDMMC_HOST_SDSPI;
  }

  // set optional defaults
  int cd_pin   = SDMMC_SLOT_NO_CD;
  int wp_pin   = SDMMC_SLOT_NO_WP;
  int freq_khz = SDMMC_FREQ_DEFAULT;
  int width    = SDMMC_HOST_FLAG_1BIT;
  // additional cs for SDSPI configuration
  int cs_pin   = -1;

  if (lua_type( L, ++stack ) == LUA_TTABLE) {
    lua_pushvalue( L, stack );
    // retrieve slot configuration from table
    cd_pin   = opt_checkint( L, "cd_pin", cd_pin );
    wp_pin   = opt_checkint( L, "wp_pin", wp_pin );
    freq_khz = opt_checkint( L, "fmax", freq_khz * 1000 ) / 1000;
    width    = opt_checkint( L, "width", width );

    // mandatory entries for SDSPI configuration
    if (is_sdspi) {
      if (is_field_present(L, -1, "sck_pin") ||
          is_field_present(L, -1, "mosi_pin") ||
          is_field_present(L, -1, "miso_pin"))
      {
        platform_print_deprecation_note("SCK/MOSI/MISO ignored; please configure via spimaster instead", "IDFv5");
      }
      cs_pin   = opt_checkint_range( L, "cs_pin", -1, 0, GPIO_NUM_MAX );
    }

    lua_settop( L, top );
  } else {
    // table is optional for SDMMC drivers, but mandatory for SD-SPI
    if (is_sdspi)
      return luaL_error( L, "missing slot configuration table" );
  }

  // find first free card
  int card_idx;
  for (card_idx = 0; card_idx < NUM_CARDS; card_idx++) {
    if (lsdmmc_card[card_idx] == NULL) {
      lsdmmc_card[card_idx] = (sdmmc_card_t *)luaM_malloc( L, sizeof( sdmmc_card_t ) );
      break;
    }
  }
  if (card_idx == NUM_CARDS)
    return luaL_error( L, "too many cards" );

  // initialize host
  // tolerate error due to re-initialization
  esp_err_t res;

  if (is_sdspi) {
    res = sdspi_host_init();
  } else {
    res = sdmmc_host_init();
  }
  if (res == ESP_OK || res == ESP_ERR_INVALID_STATE) {

    sdmmc_host_t sdspi_host_config = SDSPI_HOST_DEFAULT();
    sdmmc_host_t sdmmc_host_config = SDMMC_HOST_DEFAULT();

    if (is_sdspi) {
      // configure SDSPI slot
      sdspi_device_config_t dev_config = SDSPI_DEVICE_CONFIG_DEFAULT();
      dev_config.gpio_cs = cs_pin;
      dev_config.gpio_cd = cd_pin;
      dev_config.gpio_wp = wp_pin;
      res = sdspi_host_init_device(&dev_config, &sdspi_host_config.slot);
    } else {
      // configure SDMMC slot
      sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
      slot_config.gpio_cd = cd_pin;
      slot_config.gpio_wp = wp_pin;
      res = sdmmc_host_init_slot( slot, &slot_config );
    }
    if (res == ESP_OK) {
      // initialize card
      sdmmc_host_t *host_config = is_sdspi ? &sdspi_host_config : &sdmmc_host_config;
      host_config->slot = slot;
      host_config->flags &= ~SDMMC_HOST_FLAG_8BIT;
      host_config->flags |= width;
      host_config->max_freq_khz = freq_khz;
      if (sdmmc_card_init( host_config, lsdmmc_card[card_idx] ) == ESP_OK) {

        lsdmmc_ud_t *ud = (lsdmmc_ud_t *)lua_newuserdata( L, sizeof( lsdmmc_ud_t ) );
        if (ud) {
          luaL_getmetatable(L, "sdmmc.card");
          lua_setmetatable(L, -2);
          ud->card = lsdmmc_card[card_idx];
          ud->base_path_ref = LUA_NOREF;
          ud->fs = NULL;
          ud->pdrv = 0;
          // all done
          return 1;
        }

      } else
        err_msg = "failed to init card";

    } else
      err_msg = "failed to init slot";

    if (is_sdspi ) {
      sdspi_host_deinit();
    } else {
      sdmmc_host_deinit();
    }
  } else
    err_msg = "failed to init sdmmc host";

  luaM_free(L, lsdmmc_card[card_idx]);
  lsdmmc_card[card_idx] = NULL;

  return luaL_error( L, err_msg );
}


#define GET_CARD_UD \
  lsdmmc_ud_t *ud = (lsdmmc_ud_t *)luaL_checkudata( L, 1, "sdmmc.card" ); \
  sdmmc_card_t *card = ud->card;


// Lua: data = card:read( start_sec, num_sec )
static int lsdmmc_read( lua_State * L)
{
  const char *err_msg = "";
  GET_CARD_UD;
  int stack = 1;

  int start_sec = luaL_checkint( L, ++stack );
  luaL_argcheck( L, start_sec >= 0, stack, "out of range" );

  int num_sec = luaL_optint( L, ++stack, 1 );
  luaL_argcheck( L, num_sec >= 0, stack, "out of range" );

  // get read buffer
  const size_t rbuf_size = num_sec * 512;
  char *rbuf = luaM_malloc( L, rbuf_size );

  if (sdmmc_read_sectors( card, rbuf, start_sec, num_sec ) == ESP_OK) {
    luaL_Buffer b;
    luaL_buffinit( L, &b );
    luaL_addlstring( &b, rbuf, num_sec * 512 );
    luaL_pushresult( &b );

    luaN_freearray( L, rbuf, rbuf_size );

    // all ok
    return 1;

  } else
    err_msg = "card access failed";

  luaN_freearray( L, rbuf, rbuf_size );
  return luaL_error( L, err_msg );
}

// Lua: card:write( start_sec, data )
static int lsdmmc_write( lua_State * L)
{
  const char *err_msg = "";
  GET_CARD_UD;
  int stack = 1;

  int start_sec = luaL_checkint( L, ++stack );
  luaL_argcheck( L, start_sec >= 0, stack, "out of range" );

  size_t len;
  const char *wbuf = luaL_checklstring( L, ++stack, &len );
  luaL_argcheck( L, len % 512 == 0, stack, "must be multiple of 512" );

  if (sdmmc_write_sectors( card, wbuf, start_sec, len / 512 ) == ESP_OK) {
    // all ok
    return 0;
  } else
    err_msg = "card access failed";

  return luaL_error( L, err_msg );
}

#define SET_INT_FIELD(item, elem)        \
  lua_pushinteger( L, card->item.elem ); \
  lua_setfield( L, -2, #elem );

// Lua: info = card:get_info()
static int lsdmmc_get_info( lua_State *L )
{
  GET_CARD_UD;

  lua_newtable( L );

  // OCR
  lua_pushinteger( L, card->ocr );
  lua_setfield( L, -2, "ocr" );

  // CID
  lua_newtable( L );
  SET_INT_FIELD(cid, mfg_id);
  SET_INT_FIELD(cid, oem_id);
  SET_INT_FIELD(cid, revision);
  SET_INT_FIELD(cid, serial);
  SET_INT_FIELD(cid, date);
  lua_pushstring( L, card->cid.name );
  lua_setfield( L, -2, "name" );
  //
  lua_setfield( L, -2, "cid" );

  // CSD
  lua_newtable( L );
  SET_INT_FIELD(csd, csd_ver);
  SET_INT_FIELD(csd, mmc_ver);
  SET_INT_FIELD(csd, capacity);
  SET_INT_FIELD(csd, sector_size);
  SET_INT_FIELD(csd, read_block_len);
  SET_INT_FIELD(csd, card_command_class);
  SET_INT_FIELD(csd, tr_speed);
  //
  lua_setfield( L, -2, "csd" );

  // SCR
  lua_newtable( L );
  SET_INT_FIELD(scr, sd_spec);
  SET_INT_FIELD(scr, bus_width);
  //
  lua_setfield( L, -2, "scr" );

  // RCA
  lua_pushinteger( L, card->rca );
  lua_setfield( L, -2, "rca" );

  return 1;
}


// Lua: card:mount("/SD0"[, partition])
static int lsdmmc_mount( lua_State *L )
{
  GET_CARD_UD;
  (void)card;

  const char *ldrv = luaL_checkstring(L, 2);
  if (!lua_isnoneornil(L, 3))
  {
    // Warn that this feature isn't around
    platform_print_deprecation_note(
      "partition selection not supported", "IDFv5");
    // ...and error if used to select something we can no longer do
    if (luaL_optint(L, 3, 0) > 1)
      return luaL_error(L, "only first partition supported since IDFv5");
  }

  lua_settop(L, 2);

  if (ud->fs == NULL)
  {
    esp_err_t err = sdmmc_mount_fat(ud, ldrv);
    if (err == ESP_OK)
    {
      // We need this when we unmount
      ud->base_path_ref = luaL_ref(L, LUA_REGISTRYINDEX);
      lua_pushboolean(L, true);
    }
    else
      lua_pushboolean(L, false);
    return 1;
  }
  else
    return luaL_error(L, "already mounted");
}


// Lua: card:umount()
static int lsdmmc_umount( lua_State *L )
{
  GET_CARD_UD;
  (void)card;

  if (ud->fs)
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->base_path_ref);
    const char *base_path = lua_tostring(L, -1);
    luaL_unref2(L, LUA_REGISTRYINDEX, ud->base_path_ref);
    esp_err_t err = sdmmc_unmount_fat(ud, base_path);
    if (err != ESP_OK)
      return luaL_error(L, "unmount reported error code %d", err);
    return 0;
  }
  else
    return luaL_error(L, "not mounted");
}


LROT_BEGIN(sdmmc_card, NULL, LROT_MASK_INDEX)
  LROT_TABENTRY( __index,   sdmmc_card )
  LROT_FUNCENTRY( read,     lsdmmc_read )
  LROT_FUNCENTRY( write,    lsdmmc_write )
  LROT_FUNCENTRY( get_info, lsdmmc_get_info )
  LROT_FUNCENTRY( mount,    lsdmmc_mount )
  LROT_FUNCENTRY( umount,   lsdmmc_umount )
LROT_END(sdmmc_card, NULL, LROT_MASK_INDEX)

LROT_BEGIN(sdmmc, NULL, 0)
  LROT_FUNCENTRY( init,  lsdmmc_init )
  LROT_NUMENTRY(  HS1,   SDMMC_HOST_SLOT_0 )
  LROT_NUMENTRY(  HS2,   SDMMC_HOST_SLOT_1 )
#ifdef CONFIG_IDF_TARGET_ESP32
  LROT_NUMENTRY(  HSPI,  LSDMMC_HOST_HSPI )
  LROT_NUMENTRY(  VSPI,  LSDMMC_HOST_VSPI )
#endif
  LROT_NUMENTRY(  SPI2,  LSDMMC_HOST_SPI2 )
  LROT_NUMENTRY(  SPI3,  LSDMMC_HOST_SPI3 )
  LROT_NUMENTRY(  W1BIT, SDMMC_HOST_FLAG_1BIT )
  LROT_NUMENTRY(  W4BIT, SDMMC_HOST_FLAG_1BIT |
                         SDMMC_HOST_FLAG_4BIT )
  LROT_NUMENTRY(  W8BIT, SDMMC_HOST_FLAG_1BIT |
                         SDMMC_HOST_FLAG_4BIT |
                         SDMMC_HOST_FLAG_8BIT )
LROT_END(sdmmc, NULL, 0)

static int luaopen_sdmmc( lua_State *L )
{
  luaL_rometatable(L, "sdmmc.card", LROT_TABLEREF(sdmmc_card));

  // initialize cards
  for (int i = 0; i < NUM_CARDS; i++ ) {
    lsdmmc_card[i] = NULL;
  }

  return 0;
}

NODEMCU_MODULE(SDMMC, "sdmmc", sdmmc, luaopen_sdmmc);
