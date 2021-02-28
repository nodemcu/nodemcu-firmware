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
  uint32_t init_data[SPI_FLASH_SEC_SIZE/sizeof(uint32_t)];
  partition_item_t pd_pt = {0,0,0};
  uint32_t init_sector;

  luaL_argcheck(L, cast(uint8_t, byte107+1) < 2, 1, "Invalid mode");
  system_partition_get_item(SYSTEM_PARTITION_PHY_DATA, &pd_pt);
  init_sector = platform_flash_get_sector_of_address(pd_pt.addr);

  if (pd_pt.size == 0 ||
      platform_s_flash_read(init_data, pd_pt.addr, sizeof(init_data))==0)
    return luaL_error(L, "flash read error");

  // If it's already the correct value, we don't need to force it
  if (cast(uint8_t *, init_data)[107] == byte107) {
    lua_pushboolean (L, false);
    return 1;
  }

  cast(uint8_t *, init_data)[107] = byte107;
  /* Only do erase if toggling 0x00 to 0xFF */
  if(byte107 && platform_flash_erase_sector(init_sector) != PLATFORM_OK)
    return luaL_error(L, "flash erase error");

  if(platform_flash_write(init_data, pd_pt.addr, sizeof(init_data))==0)
    return luaL_error(L, "flash write error");

  lua_pushboolean (L, true);
  return 1;
}

// Module function map
LROT_BEGIN(adc, NULL, 0)
  LROT_FUNCENTRY( read, adc_sample )
  LROT_FUNCENTRY( readvdd33, adc_readvdd33 )
  LROT_FUNCENTRY( force_init_mode, adc_init107 )
  LROT_NUMENTRY( INIT_ADC, 0x00 )
  LROT_NUMENTRY( INIT_VDD33, 0xff )
LROT_END(adc, NULL, 0)


NODEMCU_MODULE(ADC, "adc", adc, NULL);
