// Module for interfacing with the SPI interface

#include "module.h"
#include "lauxlib.h"

#include "spi_common.h"
#include "driver/spi_common.h"


LROT_BEGIN(lspi)
  LROT_FUNCENTRY( master, lspi_master )
//  LROT_FUNCENTRY( slave,  lspi_slave )
  LROT_NUMENTRY( SPI,    SPI_HOST )
  LROT_NUMENTRY( HSPI,   HSPI_HOST )
  LROT_NUMENTRY( VSPI,   VSPI_HOST )
LROT_END(lspi, NULL, 0)

int luaopen_spi( lua_State *L ) {
  luaopen_spi_master( L );
  return 0;
}

NODEMCU_MODULE(SPI, "spi", lspi, luaopen_spi);
