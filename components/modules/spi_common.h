
#ifndef _NODEMCU_SPI_COMMON_H_
#define _NODEMCU_SPI_COMMON_H_

#include "lauxlib.h"

// ***************************************************************************
// SPI master
//
int luaopen_spi_master( lua_State *L );
int lspi_master( lua_State *L );


#endif /*_NODEMCU_SPI_COMMON_H_*/
