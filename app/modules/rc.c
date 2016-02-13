#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "rom.h"
//#include "driver/easygpio.h"
//static Ping_Data pingA;
#define defPulseLen 185
#define defProtocol 1
#define defRepeat   10
#define defBits     24
static void ICACHE_FLASH_ATTR transmit(int pin, int pulseLen, int nHighPulses, int nLowPulses) {
  platform_gpio_write(pin, 1);
  os_delay_us(pulseLen*nHighPulses);
  platform_gpio_write(pin, 0);
  os_delay_us(pulseLen*nLowPulses);
}
//rc.send(4,267715,24,185,1,10)    --GPIO, code, bits, pulselen, protocol, repeat
static int ICACHE_FLASH_ATTR rc_send(lua_State* L) {
  const uint8_t pin = luaL_checkinteger(L, 1);
  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  //platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_PULLUP);
  //platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_PULLDOWN);
  platform_gpio_write(pin, 0);
  long code = luaL_checklong(L, 2);
  //const uint8_t bits = luaL_checkinteger(L, 3);
  uint8_t bits = luaL_checkinteger(L, 3);
  const uint8_t pulseLen = luaL_checkinteger(L, 4);
  const uint8_t Protocol = luaL_checkinteger(L, 5);
  const uint8_t repeat = luaL_checkinteger(L, 6);
  NODE_ERR("pulseLen:%d\n",pulseLen);
  NODE_ERR("Protocol:%d\n",Protocol);
  NODE_ERR("repeat:%d\n",repeat);
  NODE_ERR("send:");
  int c,k,nRepeat;
  bits = bits-1;
  for (c = bits; c >= 0; c--)
  {
    k = code >> c;
    if (k & 1)
      NODE_ERR("1");
    else
      NODE_ERR("0");
  }
  NODE_ERR("\n");
  for (nRepeat=0; nRepeat<repeat; nRepeat++) {
    for (c = bits; c >= 0; c--)
    {
      k = code >> c;
      if (k & 1){
        //send1
        if(Protocol==1){
          transmit(pin,pulseLen,3,1);
        }else if(Protocol==2){
          transmit(pin,pulseLen,2,1);
        }else if(Protocol==3){
          transmit(pin,pulseLen,9,6);
        }
      }
      else{
        //send0
        if(Protocol==1){
          transmit(pin,pulseLen,1,3);
        }else if(Protocol==2){
          transmit(pin,pulseLen,1,2);
        }else if(Protocol==3){
          transmit(pin,pulseLen,4,11);
        }
      }
    }
    //sendSync();
    if(Protocol==1){
      transmit(pin,pulseLen,1,31);
    }else if(Protocol==2){
      transmit(pin,pulseLen,1,10);
    }else if(Protocol==3){
      transmit(pin,pulseLen,1,71);
    }
  }

  return 1;
}

// Module function map
static const LUA_REG_TYPE rc_map[] = {
  { LSTRKEY( "send" ), LFUNCVAL( rc_send )},
  { LNILKEY, LNILVAL}
};

int luaopen_rc(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  return 0;
}

NODEMCU_MODULE(RC, "rc", rc_map, luaopen_rc);
