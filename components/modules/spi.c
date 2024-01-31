// Module for interfacing with the SPI interface

#include "module.h"
#include "lauxlib.h"

#include "spi_common.h"
#include "driver/spi_common.h"


LROT_BEGIN(lspi, NULL, 0)
  LROT_FUNCENTRY( master, lspi_master )
//  LROT_FUNCENTRY( slave,  lspi_slave )
#if defined(CONFIG_IDF_TARGET_ESP32)
  LROT_NUMENTRY( SPI,    SPI_HOST )
  LROT_NUMENTRY( HSPI,   HSPI_HOST )
  LROT_NUMENTRY( VSPI,   VSPI_HOST )
#endif
  LROT_NUMENTRY( SPI1,   SPI1_HOST )
  LROT_NUMENTRY( SPI2,   SPI2_HOST )
#ifdef SPI3_HOST
  LROT_NUMENTRY( SPI3,   SPI3_HOST )
#endif
LROT_END(lspi, NULL, 0)

int luaopen_spi( lua_State *L ) {
  luaopen_spi_master( L );
  return 0;
}

NODEMCU_MODULE(SPI, "spi", lspi, luaopen_spi);
