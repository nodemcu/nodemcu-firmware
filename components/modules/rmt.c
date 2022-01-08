// Module for working with the rmt driver

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "driver/rmt.h"

#include "common.h"

static int lrmt_txsetup(lua_State *L) {
  return 0;
}

static int lrmt_rxsetup(lua_State *L) {
  return 0;
}

static int lrmt_on(lua_State *L) {
  return 0;
}

static int lrmt_rx_close(lua_State *L) {
  return 0;
}

static int lrmt_send(lua_State *L) {
  return 0;
}

static int lrmt_tx_close(lua_State *L) {
  return 0;
}

// Module function map
LROT_BEGIN(rmt_rx, NULL, LROT_MASK_INDEX)
  LROT_FUNCENTRY( __gc,            lrmt_rx_close )
  LROT_TABENTRY ( __index,         rmt_rx )
  LROT_FUNCENTRY( on,              lrmt_on )
  LROT_FUNCENTRY( close,           lrmt_rx_close )
LROT_END(rmt_rx, NULL, LROT_MASK_INDEX)

LROT_BEGIN(rmt_tx, NULL, LROT_MASK_INDEX)
  LROT_FUNCENTRY( __gc,            lrmt_tx_close )
  LROT_TABENTRY ( __index,         rmt_tx )
  LROT_FUNCENTRY( send,            lrmt_send )
  LROT_FUNCENTRY( close,           lrmt_tx_close )
LROT_END(rmt_tx, NULL, LROT_MASK_INDEX)

LROT_BEGIN(rmt, NULL, 0)
  LROT_TABENTRY ( __index,         rmt )
  LROT_FUNCENTRY( txsetup,         lrmt_txsetup )
  LROT_FUNCENTRY( rxsetup,         lrmt_rxsetup )
LROT_END(rmt, NULL, 0)

int luaopen_rmt(lua_State *L) {
  luaL_rometatable(L, "rmt.tx", LROT_TABLEREF(rmt_tx));  // create metatable
  luaL_rometatable(L, "rmt.rx", LROT_TABLEREF(rmt_rx));  // create metatable
  return 0;
}

NODEMCU_MODULE(RMT, "rmt", rmt, luaopen_rmt);
