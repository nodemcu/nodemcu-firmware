// Module for interfacing with adc

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include <stdint.h>
#include "user_interface.h"

// Lua: read(id) , return system adc
static int adc_sample( lua_State* L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( adc, id );
  unsigned val = 0xFFFF & system_adc_read();
  lua_pushinteger( L, val );
  return 1;
}

// Lua: readvdd33()
static int adc_readvdd33( lua_State* L )
{
  lua_pushinteger(L, system_get_vdd33 ());
  return 1;
}

// Lua: adc.force_init_mode(x)
static int adc_init107( lua_State *L )
{
  uint8_t byte107 = luaL_checkinteger (L, 1);

  uint32 init_sector = flash_rom_get_sec_num () - 4;

  // Note 32bit alignment so we can safely cast to uint32 for the flash api
  char init_data[SPI_FLASH_SEC_SIZE] __attribute__((aligned(4)));

  if (SPI_FLASH_RESULT_OK != flash_read (
    init_sector * SPI_FLASH_SEC_SIZE,
    (uint32 *)init_data, sizeof(init_data)))
      return luaL_error(L, "flash read error");

  // If it's already the correct value, we don't need to force it
  if (init_data[107] == byte107)
  {
    lua_pushboolean (L, false);
    return 1;
  }

  // Nope, it differs, we need to rewrite it
  init_data[107] = byte107;
  if (SPI_FLASH_RESULT_OK != flash_erase (init_sector))
    return luaL_error(L, "flash erase error");

  if (SPI_FLASH_RESULT_OK != flash_write (
    init_sector * SPI_FLASH_SEC_SIZE,
    (uint32 *)init_data, sizeof(init_data)))
      return luaL_error(L, "flash write error");

  lua_pushboolean (L, true);
  return 1;
}

// Module function map
LROT_BEGIN(adc)
  LROT_FUNCENTRY( read, adc_sample )
  LROT_FUNCENTRY( readvdd33, adc_readvdd33 )
  LROT_FUNCENTRY( force_init_mode, adc_init107 )
  LROT_NUMENTRY( INIT_ADC, 0x00 )
  LROT_NUMENTRY( INIT_VDD33, 0xff )
LROT_END( adc, NULL, 0 )


NODEMCU_MODULE(ADC, "adc", adc, NULL);
