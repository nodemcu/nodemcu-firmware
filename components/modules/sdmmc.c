
#include "module.h"
#include "lauxlib.h"

#include "lmem.h"

#include "vfs.h"

#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"

#include "common.h"

// the index of cards here must be aligned with the pdrv for fatfs
// see FF_VOLUMES in ffconf.h
#define NUM_CARDS 4
sdmmc_card_t *lsdmmc_card[NUM_CARDS];

// local definition for SDSPI host
#define LSDMMC_HOST_SDSPI 100
#define LSDMMC_HOST_HSPI (LSDMMC_HOST_SDSPI + HSPI_HOST)
#define LSDMMC_HOST_VSPI (LSDMMC_HOST_SDSPI + VSPI_HOST)

typedef struct {
  sdmmc_card_t *card;
  vfs_vol *vol;
} lsdmmc_ud_t;


static int lsdmmc_init( lua_State *L )
{
  const char *err_msg = "";
  int stack = 0;
  int top = lua_gettop( L );

  int slot = luaL_checkint( L, ++stack );
  luaL_argcheck( L, slot == SDMMC_HOST_SLOT_0 || slot == SDMMC_HOST_SLOT_1 ||
                    slot == LSDMMC_HOST_HSPI || slot == LSDMMC_HOST_VSPI,
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
  // additional entries for SDSPI configuration
  int sck_pin  = -1;
  int mosi_pin = -1;
  int miso_pin = -1;
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
      sck_pin  = opt_checkint_range( L, "sck_pin", -1, 0, GPIO_NUM_MAX );
      mosi_pin = opt_checkint_range( L, "mosi_pin", -1, 0, GPIO_NUM_MAX );
      miso_pin = opt_checkint_range( L, "miso_pin", -1, 0, GPIO_NUM_MAX );
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

    if (is_sdspi) {
      // configure SDSPI slot
      sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
      slot_config.gpio_miso = miso_pin;
      slot_config.gpio_mosi = mosi_pin;
      slot_config.gpio_sck = sck_pin;
      slot_config.gpio_cs = cs_pin;
      slot_config.gpio_cd = cd_pin;
      slot_config.gpio_wp = wp_pin;
      res = sdspi_host_init_slot( slot, &slot_config );
    } else {
      // configure SDMMC slot
      sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
      slot_config.gpio_cd = cd_pin;
      slot_config.gpio_wp = wp_pin;
      res = sdmmc_host_init_slot( slot, &slot_config );
    }
    if (res == ESP_OK) {

      // initialize card
      sdmmc_host_t sdspi_host_config = SDSPI_HOST_DEFAULT();
      sdmmc_host_t sdmmc_host_config = SDMMC_HOST_DEFAULT();
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
          ud->vol = NULL;

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

    luaM_freearray( L, rbuf, rbuf_size, uint8_t );

    // all ok
    return 1;

  } else
    err_msg = "card access failed";

  luaM_freearray( L, rbuf, rbuf_size, uint8_t );
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
  const char *err_msg = "";

  GET_CARD_UD;
  (void)card;

  int stack = 1;

  const char *ldrv = luaL_checkstring( L, ++stack );

  int num = luaL_optint( L, ++stack, -1 );

  if (ud->vol == NULL) {

    if ((ud->vol = vfs_mount( ldrv, num )))
      lua_pushboolean( L, true );
    else
      lua_pushboolean( L, false );

    return 1;

  } else
    err_msg = "already mounted";

  return luaL_error( L, err_msg );
}

// Lua: card:umount()
static int lsdmmc_umount( lua_State *L )
{
  const char *err_msg = "";

  GET_CARD_UD;
  (void)card;

  if (ud->vol) {
    if (vfs_umount( ud->vol ) == VFS_RES_OK) {
      ud->vol = NULL;
      // all ok
      return 0;
    } else
      err_msg = "umount failed";

  } else
    err_msg = "not mounted";

  return luaL_error( L, err_msg );
}

static const LUA_REG_TYPE sdmmc_card_map[] = {
  { LSTRKEY( "read" ),     LFUNCVAL( lsdmmc_read ) },
  { LSTRKEY( "write" ),    LFUNCVAL( lsdmmc_write ) },
  { LSTRKEY( "get_info" ), LFUNCVAL( lsdmmc_get_info ) },
  { LSTRKEY( "mount" ),    LFUNCVAL( lsdmmc_mount ) },
  { LSTRKEY( "umount" ),   LFUNCVAL( lsdmmc_umount ) },
  { LSTRKEY( "__index" ),  LROVAL( sdmmc_card_map ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE sdmmc_map[] = {
  { LSTRKEY( "init" ),  LFUNCVAL( lsdmmc_init ) },
  { LSTRKEY( "HS1" ),   LNUMVAL( SDMMC_HOST_SLOT_0 ) },
  { LSTRKEY( "HS2" ),   LNUMVAL( SDMMC_HOST_SLOT_1 ) },
  { LSTRKEY( "HSPI" ),  LNUMVAL( LSDMMC_HOST_HSPI ) },
  { LSTRKEY( "VSPI" ),  LNUMVAL( LSDMMC_HOST_VSPI ) },
  { LSTRKEY( "W1BIT" ), LNUMVAL( SDMMC_HOST_FLAG_1BIT ) },
  { LSTRKEY( "W4BIT" ), LNUMVAL( SDMMC_HOST_FLAG_1BIT |
                                 SDMMC_HOST_FLAG_4BIT ) },
  { LSTRKEY( "W8BIT" ), LNUMVAL( SDMMC_HOST_FLAG_1BIT |
                                 SDMMC_HOST_FLAG_4BIT |
                                 SDMMC_HOST_FLAG_8BIT ) },
  { LNILKEY, LNILVAL }
};

static int luaopen_sdmmc( lua_State *L )
{
  luaL_rometatable(L, "sdmmc.card", (void *)sdmmc_card_map);

  // initialize cards
  for (int i = 0; i < NUM_CARDS; i++ ) {
    lsdmmc_card[i] = NULL;
  }

  return 0;
}

NODEMCU_MODULE(SDMMC, "sdmmc", sdmmc_map, luaopen_sdmmc);
