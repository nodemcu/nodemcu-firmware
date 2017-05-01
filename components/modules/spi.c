// Module for interfacing with the SPI interface

#include "module.h"
#include "lauxlib.h"

#include "spi_common.h"
#include "driver/spi_common.h"


static const LUA_REG_TYPE lspi_map[] = {
  { LSTRKEY( "master" ), LFUNCVAL( lspi_master ) },
//  { LSTRKEY( "slave" ),  LFUNCVAL( lspi_slave ) },
  { LSTRKEY( "SPI" ),    LNUMVAL( SPI_HOST ) },
  { LSTRKEY( "HSPI" ),   LNUMVAL( HSPI_HOST ) },
  { LSTRKEY( "VSPI" ),   LNUMVAL( VSPI_HOST ) },
  {LNILKEY, LNILVAL}
};

int luaopen_spi( lua_State *L ) {
  luaopen_spi_master( L );
  return 0;
}

NODEMCU_MODULE(SPI, "spi", lspi_map, luaopen_spi);
