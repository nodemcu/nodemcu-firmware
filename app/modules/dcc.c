// NodeMCU Lua port by @voborsky, @pjsg
// Module for handling NMRA DCC protocol
// #define NODE_DEBUG

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "driver/NmraDcc.h"
#include "hw_timer.h"

#ifdef LUA_USE_MODULES_DCC
#if !defined(GPIO_INTERRUPT_ENABLE) || !defined(GPIO_INTERRUPT_HOOK_ENABLE)
#error Must have GPIO_INTERRUPT and GPIO_INTERRUPT_HOOK if using DCC module
#endif
#endif

#define TYPE "Type"
#define OPERATION "Operation"

#define TIMER_OWNER (('D' << 8) + 'C')

static inline void register_lua_cb(lua_State* L,int* cb_ref){
  int ref=luaL_ref(L, LUA_REGISTRYINDEX);
  if( *cb_ref != LUA_NOREF){
    luaL_unref(L, LUA_REGISTRYINDEX, *cb_ref);
  }
  *cb_ref = ref;
}

static inline void unregister_lua_cb(lua_State* L, int* cb_ref){
  if(*cb_ref != LUA_NOREF){
    luaL_unref(L, LUA_REGISTRYINDEX, *cb_ref);
    *cb_ref = LUA_NOREF;
  }
}

static int notify_cb = LUA_NOREF;
static int CV_cb = LUA_NOREF;
static int CV_ref = LUA_NOREF;
static int8_t ackPin;
static char ackInProgress;
static platform_task_handle_t tasknumber;

typedef struct {
  uint16_t cv;
  uint8_t value;
} CVData;

// DCC commands

void cbInit(lua_State* L, uint16_t command) {
  if(notify_cb == LUA_NOREF)
    return;
  lua_rawgeti(L, LUA_REGISTRYINDEX, notify_cb);
  lua_pushinteger(L, command);
  lua_newtable(L);
}

void cbAddFieldInteger(lua_State* L, uint16_t Value, char *Field) {
  lua_pushinteger(L, Value);
  lua_setfield(L, -2, Field);
}

void notifyDccReset(uint8_t hardReset ) {
  lua_State* L = lua_getstate();
  cbInit(L, DCC_RESET);
  cbAddFieldInteger(L, hardReset, "hardReset");
  luaL_pcallx(L, 2, 0);
}

void notifyDccIdle(void) {
  lua_State* L = lua_getstate();
  cbInit(L, DCC_IDLE);
  luaL_pcallx(L, 2, 0);
}

void notifyDccSpeed( uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps ) {
  lua_State* L = lua_getstate();
  cbInit(L, DCC_SPEED);
  cbAddFieldInteger(L, Addr, "Addr");
  cbAddFieldInteger(L, AddrType, "AddrType");
  cbAddFieldInteger(L, Speed, "Speed");
  cbAddFieldInteger(L, Dir, "Dir");
  cbAddFieldInteger(L, SpeedSteps, "SpeedSteps");
  luaL_pcallx(L, 2, 0);
}

void notifyDccSpeedRaw( uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Raw) {
  lua_State* L = lua_getstate();
  cbInit(L, DCC_SPEED_RAW);
  cbAddFieldInteger(L, Addr, "Addr");
  cbAddFieldInteger(L, AddrType, "AddrType");
  cbAddFieldInteger(L, Raw, "Raw");
  luaL_pcallx(L, 2, 0);
}

void notifyDccFunc( uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState) {
  lua_State* L = lua_getstate();
  cbInit(L, DCC_FUNC);
  cbAddFieldInteger(L, Addr, "Addr");
  cbAddFieldInteger(L, AddrType, "AddrType");
  cbAddFieldInteger(L, FuncGrp, "FuncGrp");
  cbAddFieldInteger(L, FuncState, "FuncState");
  luaL_pcallx(L, 2, 0);
}

void notifyDccAccTurnoutBoard( uint16_t BoardAddr, uint8_t OutputPair, uint8_t Direction, uint8_t OutputPower ) {
  lua_State* L = lua_getstate();
  cbInit(L, DCC_TURNOUT);
  cbAddFieldInteger(L, BoardAddr, "BoardAddr");
  cbAddFieldInteger(L, OutputPair, "OutputPair");
  cbAddFieldInteger(L, Direction, "Direction");
  cbAddFieldInteger(L, OutputPower, "OutputPower");
  luaL_pcallx(L, 2, 0);
}

void notifyDccAccTurnoutOutput( uint16_t Addr, uint8_t Direction, uint8_t OutputPower ) { 
  lua_State* L = lua_getstate();
  cbInit(L, DCC_TURNOUT);
  cbAddFieldInteger(L, Addr, "Addr");
  cbAddFieldInteger(L, Direction, "Direction");
  cbAddFieldInteger(L, OutputPower, "OutputPower");
  luaL_pcallx(L, 2, 0);
}

void notifyDccAccBoardAddrSet( uint16_t BoardAddr) { 
  lua_State* L = lua_getstate();
  cbInit(L, DCC_ACCESSORY);
  cbAddFieldInteger(L, BoardAddr, "BoardAddr");
  luaL_pcallx(L, 2, 0);
}

void notifyDccAccOutputAddrSet( uint16_t Addr) { 
  lua_State* L = lua_getstate();
  cbInit(L, DCC_ACCESSORY);
  cbAddFieldInteger(L, Addr, "Addr");
  luaL_pcallx(L, 2, 0);
}

void notifyDccSigOutputState( uint16_t Addr, uint8_t State) { 
  lua_State* L = lua_getstate();
  cbInit(L, DCC_ACCESSORY);
  cbAddFieldInteger(L, State, "State");
  luaL_pcallx(L, 2, 0);
}

void notifyDccMsg( DCC_MSG * Msg ) { 
  lua_State* L = lua_getstate();
  cbInit(L, DCC_RAW);
  cbAddFieldInteger(L, Msg->Size, "Size");
  cbAddFieldInteger(L, Msg->PreambleBits, "PreambleBits");
  char field[8];
  for(uint8_t i = 0; i< MAX_DCC_MESSAGE_LEN; i++ ) {
    ets_sprintf(field, "Data%d", i);
    cbAddFieldInteger(L, Msg->Data[i], field);
  }
  luaL_pcallx(L, 2, 0);
}

void notifyServiceMode(bool InServiceMode){ 
  lua_State* L = lua_getstate();
  cbInit(L, DCC_SERVICEMODE);
  cbAddFieldInteger(L, InServiceMode, "InServiceMode");
  luaL_pcallx(L, 2, 0);
}

// CV handling

uint16_t notifyCVValid( uint16_t CV, uint8_t Writable ) { 
  lua_State* L = lua_getstate();
  if (CV_ref != LUA_NOREF) {
    return 1;
  }
  if(notify_cb == LUA_NOREF)
    return 0;
  lua_rawgeti(L, LUA_REGISTRYINDEX, CV_cb);
  lua_pushinteger(L, CV_VALID);
  lua_newtable(L);
  cbAddFieldInteger(L, CV, "CV");
  cbAddFieldInteger(L, Writable, "Writable");
  if (luaL_pcallx(L, 2, 1) != LUA_OK)
    return 0;
  uint8 result = lua_tointeger(L, -1) || lua_toboolean(L, -1);
  lua_pop(L, 1);
  return result;
}

static int doDirectCVRead(lua_State *L) {
  CVData *data = (CVData*) lua_touserdata(L, -1);
  lua_rawgeti(L, LUA_REGISTRYINDEX, CV_ref);
  lua_pushinteger(L, data->cv);
  lua_gettable(L, -2);
  data->value = (uint8_t) luaL_checkinteger(L, -1);
  return 0;
}

static int doDirectCVWrite(lua_State *L) {
  CVData *data = (CVData*) lua_touserdata(L, -1);
  lua_rawgeti(L, LUA_REGISTRYINDEX, CV_ref);
  lua_pushinteger(L, data->cv);
  lua_pushinteger(L, data->value);
  lua_settable(L, -3);
  return 0;
}

uint16_t notifyCVRead( uint16_t CV) { 
  lua_State* L = lua_getstate();
  if (CV_ref != LUA_NOREF) {
    CVData data;
    data.cv = CV;

    lua_pushcfunction(L, doDirectCVRead);
    lua_pushlightuserdata(L, &data);
    if (lua_pcall(L, 1, 0, 0)) {
      // An error.
      lua_pop(L, 1);
      return 256;
    }
    
    return data.value;
  }
  if(notify_cb == LUA_NOREF)
    return 0;
  lua_rawgeti(L, LUA_REGISTRYINDEX, CV_cb);
  lua_pushinteger(L, CV_READ);
  lua_newtable(L);
  cbAddFieldInteger(L, CV, "CV");
  if (luaL_pcallx(L, 2, 1) != LUA_OK)
    return 0;;
  uint8 result = lua_tointeger(L, -1);
  lua_pop(L, 1);
  return result;
}

uint16_t notifyCVWrite( uint16_t CV, uint8_t Value) { 
  lua_State* L = lua_getstate();
  if (CV_ref != LUA_NOREF) {
    CVData data;
    data.cv = CV;
    data.value = Value;

    lua_pushcfunction(L, doDirectCVWrite);
    lua_pushlightuserdata(L, &data);
    if (lua_pcall(L, 1, 0, 0)) {
      // An error.
      lua_pop(L, 1);
      return 256;
    }
    
    return data.value;
  }
  if(notify_cb == LUA_NOREF)
    return 0;
  lua_rawgeti(L, LUA_REGISTRYINDEX, CV_cb);
  lua_pushinteger(L, CV_WRITE);
  lua_newtable(L);
  cbAddFieldInteger(L, CV, "CV");
  cbAddFieldInteger(L, Value, "Value");
  luaL_pcallx(L, 2, 1);
  // Return is an optional value (if integer). If nil, then it is old style
  if (!lua_isnil(L, -1)) {
    Value = lua_tointeger(L, -1);
  }
  lua_pop(L, 1);
  return Value;
}

static void notifyCVNoArgs(int callback_type) {
  lua_State* L = lua_getstate();
  if (notify_cb == LUA_NOREF) {
    return;
  }
  lua_rawgeti(L, LUA_REGISTRYINDEX, CV_cb);
  lua_pushinteger(L, callback_type);
  luaL_pcallx(L, 1, 0);
}

void notifyCVResetFactoryDefault(void) { 
  notifyCVNoArgs(CV_RESET);
}

void notifyCVAck(void) {
  // Invoked when we should generate an ack pulse (if possible)
  if (ackPin >= 0 && !ackInProgress) {
    // Duration is 6ms +/- 1ms
    platform_hw_timer_arm_us(TIMER_OWNER, 6 * 1000);
    platform_gpio_write(ackPin, 1);
    ackInProgress = TRUE;
  }
}

static void ICACHE_RAM_ATTR cvAckComplete(os_param_t param) {
  // Invoked when we should end the ack pulse
  ackInProgress = FALSE;
  platform_gpio_write(ackPin, 0);
  if (CV_ref == LUA_NOREF) {
    platform_post_high(tasknumber, CV_ACK_COMPLETE);
  }
}

static int dcc_lua_setup(lua_State* L) {
  NODE_DBG("[dcc_lua_setup]\n");
  int narg = 1;
  uint8_t pin = luaL_checkinteger(L, narg);
  luaL_argcheck(L, platform_gpio_exists(pin) && pin>0, narg, "Invalid interrupt pin");
  narg++;

  int8_t ackpin = -1;

  if (lua_type(L, narg) == LUA_TNUMBER) {
    ackpin = luaL_checkinteger(L, narg);
    luaL_argcheck(L, platform_gpio_exists(ackpin), narg, "Invalid ack pin");
    narg++;
  } 
  
  if (lua_isfunction(L, narg)) {
    lua_pushvalue(L, narg++);
    register_lua_cb(L, &notify_cb);
  } else {
    unregister_lua_cb(L, &notify_cb);
  }

  uint8_t ManufacturerId = luaL_checkinteger(L, narg++);
  uint8_t VersionId = luaL_checkinteger(L, narg++);
  uint8_t Flags = luaL_checkinteger(L, narg++);
  uint8_t OpsModeAddressBaseCV = luaL_checkinteger(L, narg++);

  if (lua_istable(L, narg)) {
    // This is the raw CV table
    lua_pushvalue(L, narg++);
    register_lua_cb(L, &CV_ref);
  } else {
    unregister_lua_cb(L, &CV_ref);
  }

  if (lua_isfunction(L, narg)) {
    lua_pushvalue(L, narg++);
    register_lua_cb(L, &CV_cb);
  } else {
    unregister_lua_cb(L, &CV_cb);
  }

  if (ackpin >= 0) {
    // Now start things up
    if (!platform_hw_timer_init(TIMER_OWNER, FRC1_SOURCE, FALSE)) {
      // Failed to init the timer
      luaL_error(L, "Unable to initialize timer");
    }

    platform_hw_timer_set_func(TIMER_OWNER, cvAckComplete, 0);

    platform_gpio_write(ackpin, 0);
    platform_gpio_mode(ackpin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  }

  NODE_DBG("[dcc_lua_setup] Enabling interrupt on PIN %d\n", pin);
  ackPin = ackpin;
  ackInProgress = FALSE;
  dcc_setup(pin, ManufacturerId, VersionId, Flags, OpsModeAddressBaseCV );
  
  return 0;
}

static int dcc_lua_close(lua_State* L) {
  dcc_close();
  unregister_lua_cb(L, &notify_cb);
  unregister_lua_cb(L, &CV_cb);
  unregister_lua_cb(L, &CV_ref);
  return 0;
}

static void dcc_task(os_param_t param, uint8_t prio)
{
  (void) prio;

  notifyCVNoArgs(param);
}

int dcc_lua_init( lua_State *L ) {
  NODE_DBG("[dcc_lua_init]\n");
  dcc_init();
  tasknumber = platform_task_get_id(dcc_task);
  return 0;
}

// Module function map
LROT_BEGIN(dcc, NULL, 0)
  LROT_FUNCENTRY( setup, dcc_lua_setup )
  LROT_FUNCENTRY( close, dcc_lua_close )
  
  LROT_NUMENTRY( DCC_RESET, DCC_RESET )
  LROT_NUMENTRY( DCC_IDLE, DCC_IDLE )
  LROT_NUMENTRY( DCC_SPEED, DCC_SPEED )
  LROT_NUMENTRY( DCC_SPEED_RAW, DCC_SPEED_RAW )
  LROT_NUMENTRY( DCC_FUNC, DCC_FUNC )
  LROT_NUMENTRY( DCC_TURNOUT, DCC_TURNOUT )
  LROT_NUMENTRY( DCC_ACCESSORY, DCC_ACCESSORY )
  LROT_NUMENTRY( DCC_RAW, DCC_RAW )
  LROT_NUMENTRY( DCC_SERVICEMODE, DCC_SERVICEMODE )

  LROT_NUMENTRY( CV_VALID, CV_VALID )
  LROT_NUMENTRY( CV_READ, CV_READ )
  LROT_NUMENTRY( CV_WRITE, CV_WRITE )
  LROT_NUMENTRY( CV_RESET, CV_RESET )
  LROT_NUMENTRY( CV_ACK_COMPLETE, CV_ACK_COMPLETE )

  LROT_NUMENTRY( MAN_ID_JMRI, MAN_ID_JMRI)
  LROT_NUMENTRY( MAN_ID_DIY, MAN_ID_DIY)
  LROT_NUMENTRY( MAN_ID_SILICON_RAILWAY, MAN_ID_SILICON_RAILWAY)
  
  LROT_NUMENTRY( FLAGS_MY_ADDRESS_ONLY, FLAGS_MY_ADDRESS_ONLY )
  LROT_NUMENTRY( FLAGS_AUTO_FACTORY_DEFAULT, FLAGS_AUTO_FACTORY_DEFAULT )
  LROT_NUMENTRY( FLAGS_OUTPUT_ADDRESS_MODE, FLAGS_OUTPUT_ADDRESS_MODE )
  LROT_NUMENTRY( FLAGS_DCC_ACCESSORY_DECODER, FLAGS_DCC_ACCESSORY_DECODER )
LROT_END(dcc, NULL, 0)

NODEMCU_MODULE(DCC, "dcc", dcc, dcc_lua_init);
