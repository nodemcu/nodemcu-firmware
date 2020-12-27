// ***************************************************************************
// Somfy module for ESP8266 with NodeMCU
//
// Written by Lukas Voborsky, @voborsky
// based on https://github.com/Nickduino/Somfy_Remote
// Somfy protocol description: https://pushstack.wordpress.com/somfy-rts-protocol/
// and discussion: https://forum.arduino.cc/index.php?topic=208346.0
//
// MIT license, http://opensource.org/licenses/MIT
// ***************************************************************************

//#define NODE_DEBUG

#include <stdint.h>
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "task/task.h"
#include "hw_timer.h"
#include "user_interface.h"

#ifdef LUA_USE_MODULES_SOMFY
#if !defined(GPIO_INTERRUPT_ENABLE) || !defined(GPIO_INTERRUPT_HOOK_ENABLE)
#error Must have GPIO_INTERRUPT and GPIO_INTERRUPT_HOOK if using SOMFY module
#endif
#endif

#ifdef NODE_DEBUG
    #define PULLUP PLATFORM_GPIO_PULLUP
    #define OUTPUT PLATFORM_GPIO_OUTPUT
    #define HIGH PLATFORM_GPIO_HIGH
    #define LOW PLATFORM_GPIO_LOW

    #define MODE_TP1 platform_gpio_mode( 3, OUTPUT, PULLUP ); // GPIO 00
    #define SET_TP1  platform_gpio_write(3, HIGH);
    #define CLR_TP1  platform_gpio_write(3, LOW);
    #define WAIT os_delay_us(1);
#else
    #define MODE_TP1
    #define SET_TP1
    #define CLR_TP1
    #define WAIT
#endif


#define SYMBOL 640 // symbol width in microseconds
#define SOMFY_UP    0x2
#define SOMFY_STOP  0x1
#define SOMFY_DOWN  0x4
#define SOMFY_PROG  0x8

#define DIRECT_WRITE_LOW(pin)    (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), 0))
#define DIRECT_WRITE_HIGH(pin)   (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), 1))

// ----------------------------------------------------------------------------------------------------//
// ------------------------------- transmitter part ---------------------------------------------------//
// ----------------------------------------------------------------------------------------------------//
static const os_param_t TIMER_OWNER = 0x736f6d66; // "somf"
static task_handle_t SendDone_taskid;

static uint8_t    TxPin;
static uint8_t    frame[7];
static uint8_t    sync;
static uint8_t    repeat;

//static uint32_t   delay[10] = {9415, 89565, 4*SYMBOL, 4*SYMBOL, 4*SYMBOL, 4550, SYMBOL, SYMBOL, SYMBOL, 30415}; // inc us
// the `delay` array of constants must be in RAM as it is accessed from the timer interrupt
static const uint32_t delay[10] = {US_TO_RTC_TIMER_TICKS(9415), US_TO_RTC_TIMER_TICKS(89565), US_TO_RTC_TIMER_TICKS(4*SYMBOL), US_TO_RTC_TIMER_TICKS(4*SYMBOL), US_TO_RTC_TIMER_TICKS(4*SYMBOL), US_TO_RTC_TIMER_TICKS(4550), US_TO_RTC_TIMER_TICKS(SYMBOL), US_TO_RTC_TIMER_TICKS(SYMBOL), US_TO_RTC_TIMER_TICKS(SYMBOL), US_TO_RTC_TIMER_TICKS(30415)}; // in ticks (no need to recalculate)

static uint8_t    repeatindex;
static uint8_t    signalindex;
static uint8_t    subindex;
static uint8_t    bitcondition;

static int lua_done_ref = LUA_NOREF; // callback when transmission is done

void buildFrame(uint8_t *frame, uint64_t remote, uint8_t button, uint16_t code) {
    // NODE_DBG("remote: %x\n", remote);
    // NODE_DBG("button: %x\n", button);
    // NODE_DBG("rolling code: %x\n", code);
    frame[0] = 0xA7; // Encryption key. Doesn't matter much
    frame[1] = button << 4; // Which button did you press? The 4 LSB will be the checksum
    frame[2] = code >> 8; // Rolling code (big endian)
    frame[3] = code;         // Rolling code
    frame[4] = remote >> 16; // Remote address
    frame[5] = remote >> 8; // Remote address
    frame[6] = remote;         // Remote address
    // frame[7] = 0x80;
    // frame[8] = 0x0;
    // frame[9] = 0x0;

    // NODE_DBG("Frame:\t\t\t%02x %02x %02x %02x %02x %02x %02x\n", frame[0], frame[1], frame[2], frame[3], frame[4], frame[5], frame[6]);
    // Checksum calculation: a XOR of all the nibbles
    uint8_t checksum = 0;
    for(uint8_t i = 0; i < 7; i++) {
        checksum = checksum ^ frame[i] ^ (frame[i] >> 4);
    }
    checksum &= 0b1111; // We keep the last 4 bits only

    //Checksum integration
    frame[1] |= checksum; //    If a XOR of all the nibbles is equal to 0, the blinds will consider the checksum ok.
    // NODE_DBG("With checksum:\t%02x %02x %02x %02x %02x %02x %02x\n", frame[0], frame[1], frame[2], frame[3], frame[4], frame[5], frame[6]);

    // Obfuscation: a XOR of all the uint8_ts
    for(uint8_t i = 1; i < 7; i++) {
        frame[i] ^= frame[i-1];
    }
    // NODE_DBG("Obfuscated:\t\t%02x %02x %02x %02x %02x %02x %02x\n", frame[0], frame[1], frame[2], frame[3], frame[4], frame[5], frame[6]);
}

static void ICACHE_RAM_ATTR sendCommand(os_param_t p) {
    (void) p;
    //  NODE_DBG("%d\t%d\n", signalindex, subindex);
    switch (signalindex) {
        case 0:
            subindex = 0;
            if(sync == 2) { // Only with the first frame.
                //Wake-up pulse & Silence
                DIRECT_WRITE_HIGH(TxPin);
                signalindex++;
                // delayMicroseconds(9415);
                break;
            } else {
                signalindex++; signalindex++; //no break means: go directly to step 3
            }
        case 1:
            //Wake-up pulse & Silence
            DIRECT_WRITE_LOW(TxPin);
            signalindex++;
            // delayMicroseconds(89565);
            break;
        case 2:
            signalindex++;
            // no break means go directly to step 3
            // a "useless" step to allow repeating the hardware sync w/o the silence after wake-up pulse
        case 3:
            // Hardware sync: two sync for the first frame, seven for the following ones.
            DIRECT_WRITE_HIGH(TxPin);
            signalindex++;
            // delayMicroseconds(4*SYMBOL);
            break;
        case 4:
            DIRECT_WRITE_LOW(TxPin);
            subindex++;
            if (subindex < sync) {signalindex--;} else {signalindex++;}
            // delayMicroseconds(4*SYMBOL);
            break;
        case 5:
            // Software sync
            DIRECT_WRITE_HIGH(TxPin);
            signalindex++;
            // delayMicroseconds(4550);
            break;
        case 6:
            DIRECT_WRITE_LOW(TxPin);
            signalindex++;
            subindex=0;
            // delayMicroseconds(SYMBOL);
            break;
        case 7:
            //Data: bits are sent one by one, starting with the MSB.
            bitcondition = ((frame[subindex/8] >> (7 - (subindex%8))) & 1) == 1;
            if(bitcondition) {
                DIRECT_WRITE_LOW(TxPin);
            }
            else {
                DIRECT_WRITE_HIGH(TxPin);
            }
            signalindex++;
            // delayMicroseconds(SYMBOL);
            break;
        case 8:
            //Data: bits are sent one by one, starting with the MSB.
            if(bitcondition) {
                DIRECT_WRITE_HIGH(TxPin);
            }
            else {
                DIRECT_WRITE_LOW(TxPin);
            }

            if (subindex<56) {
                subindex++;
                signalindex--;
            }
            else {
                signalindex++;
            }
            // delayMicroseconds(SYMBOL);
            break;
        case 9:
            DIRECT_WRITE_LOW(TxPin);
            signalindex++;
            // delayMicroseconds(30415); // Inter-frame silence
            break;
        case 10:
            repeatindex++;
            if (repeatindex<repeat) {
                DIRECT_WRITE_HIGH(TxPin); //start repeat from step 3, but don't wait as after step 1
                signalindex=4; subindex=0; sync=7;
            } else {
                platform_hw_timer_close(TIMER_OWNER);
                if (lua_done_ref != LUA_NOREF) {
                    task_post_low (SendDone_taskid, (task_param_t)0);
                }
            }
            break;
    }
    if (signalindex<10) {
        platform_hw_timer_arm_ticks(TIMER_OWNER, delay[signalindex-1]);
    }
}

// ----------------------------------------------------------------------------------------------------//
// ------------------------------- receiver part ------------------------------------------------------//
// ----------------------------------------------------------------------------------------------------//
#define TOLERANCE_MIN 0.7
#define TOLERANCE_MAX 1.3

static const  uint32_t tempo_wakeup_pulse = 9415;
static const  uint32_t tempo_wakeup_silence = 89565;
// static const  uint32_t tempo_synchro_hw = SYMBOL*4;
static const  uint32_t tempo_synchro_hw_min = SYMBOL*4*TOLERANCE_MIN;
static const  uint32_t tempo_synchro_hw_max = SYMBOL*4*TOLERANCE_MAX;
// static const  uint32_t k_tempo_synchro_sw = 4550;
static const  uint32_t tempo_synchro_sw_min = 4550*TOLERANCE_MIN;
static const  uint32_t tempo_synchro_sw_max = 4550*TOLERANCE_MAX;
// static const  uint32_t tempo_half_symbol = SYMBOL;
static const  uint32_t tempo_half_symbol_min = SYMBOL*TOLERANCE_MIN;
static const  uint32_t tempo_half_symbol_max = SYMBOL*TOLERANCE_MAX;
// static const  uint32_t tempo_symbol = SYMBOL*2;
static const  uint32_t tempo_symbol_min = SYMBOL*2*TOLERANCE_MIN;
static const  uint32_t tempo_symbol_max = SYMBOL*2*TOLERANCE_MAX;
static const  uint32_t tempo_inter_frame_gap = 30415;

static int16_t  bitMin = SYMBOL*TOLERANCE_MIN;

typedef enum {
  waiting_synchro = 0,
  receiving_data = 1,
  complete = 2
}
t_status;

static struct SomfyRx_t
{
  t_status status;
  uint8_t cpt_synchro_hw;
  uint8_t cpt_bits;
  uint8_t previous_bit;
  bool waiting_half_symbol;
  uint8_t payload[9];
} SomfyRx;

static task_handle_t  DataReady_taskid;
static uint8_t RxPin;
static uint8_t IntBitmask;
static int lua_dataready_ref = LUA_NOREF;

static uint32_t ICACHE_RAM_ATTR InterruptHandler (uint32_t ret_gpio_status) {
  // This function really is running at interrupt level with everything
  // else masked off. It should take as little time as necessary.

  uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
  if ((gpio_status & IntBitmask) == 0) {
    return ret_gpio_status;
  }

  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & IntBitmask);
  uint32_t actMicros = system_get_time();
  ret_gpio_status &= ~(IntBitmask);

  static unsigned long  lastMicros = 0;
  unsigned long  bitMicros;
  bitMicros = actMicros-lastMicros;
  if ( bitMicros < bitMin ) {
      // too short - may be false interrupt due to glitch or false protocol -> ignore
      return ret_gpio_status; // abort IRQ
  }
  lastMicros = actMicros;

  switch(SomfyRx.status) {
    case waiting_synchro:
      if (bitMicros > tempo_synchro_hw_min && bitMicros < tempo_synchro_hw_max) {
        SET_TP1 WAIT CLR_TP1 WAIT SET_TP1
        ++SomfyRx.cpt_synchro_hw;
        CLR_TP1
      }
      else if (bitMicros > tempo_synchro_sw_min && bitMicros <  tempo_synchro_sw_max && SomfyRx.cpt_synchro_hw >= 4) {
        SET_TP1 //WAIT CLR_TP1 WAIT SET_TP1 WAIT CLR_TP1 WAIT SET_TP1 WAIT CLR_TP1 WAIT SET_TP1
        memset( &SomfyRx, 0, sizeof( SomfyRx) );
        SomfyRx.status = receiving_data;
      } else {
        SomfyRx.cpt_synchro_hw = 0;
      }
      break;

    case receiving_data:
      if (bitMicros > tempo_symbol_min && bitMicros < tempo_symbol_max && !SomfyRx.waiting_half_symbol) {
        SET_TP1
        SomfyRx.previous_bit = 1 - SomfyRx.previous_bit;
        SomfyRx.payload[SomfyRx.cpt_bits/8] += SomfyRx.previous_bit << (7 - SomfyRx.cpt_bits%8);
        ++SomfyRx.cpt_bits;
      } else if (bitMicros > tempo_half_symbol_min && bitMicros < tempo_half_symbol_max) {
        SET_TP1 WAIT CLR_TP1 WAIT SET_TP1 WAIT CLR_TP1 WAIT SET_TP1
        if (SomfyRx.waiting_half_symbol) {
          SomfyRx.waiting_half_symbol = false;
          SomfyRx.payload[SomfyRx.cpt_bits/8] += SomfyRx.previous_bit << (7 - SomfyRx.cpt_bits%8);
          ++SomfyRx.cpt_bits;
        } else {
          SomfyRx.waiting_half_symbol = true;
        }
      } else {
        SomfyRx.cpt_synchro_hw = 0;
        SomfyRx.status = waiting_synchro;
      }
      CLR_TP1
      break;

    default:
      break;
  }

  if (SomfyRx.status == receiving_data && SomfyRx.cpt_bits == 80) { //56) { experiment
    task_post_high(DataReady_taskid, (task_param_t)0);
    SomfyRx.status = waiting_synchro;
  }

  return ret_gpio_status;
}

static void somfy_decode (os_param_t param, uint8_t prio)
{
  #ifdef NODE_DEBUG
  NODE_DBG("Payload:\t");
  for(uint8_t i = 0; i < 10; i++) {
    NODE_DBG("%02x ", SomfyRx.payload[i]);
  }
  NODE_DBG("\n");
  #endif

  // Deobfuscation
  uint8_t frame[10];
  frame[0] = SomfyRx.payload[0];
  for(int i = 1; i < 7; ++i) frame[i] = SomfyRx.payload[i] ^ SomfyRx.payload[i-1];

  frame[7] = SomfyRx.payload[7] ^ SomfyRx.payload[0];
  for(int i = 8; i < 10; ++i) frame[i] = SomfyRx.payload[i] ^ SomfyRx.payload[i-1];

  #ifdef NODE_DEBUG
  NODE_DBG("Frame:\t");
  for(uint8_t i = 0; i < 10; i++) {
    NODE_DBG("%02x ", frame[i]);
  }
  NODE_DBG("\n");
  #endif

  // Checksum check
  uint8_t cksum = 0;
  for(int i = 0; i < 7; ++i) cksum = cksum ^ frame[i] ^ (frame[i] >> 4);
  cksum = cksum & 0x0F;
  if (cksum != 0) {
    NODE_DBG("Checksum incorrect!\n");
    return;
  }

  unsigned long rolling_code = (frame[2] << 8) || frame[3];
  unsigned long address = ((unsigned long)frame[4] << 16) || (frame[5] << 8) || frame[6];

  if (lua_dataready_ref == LUA_NOREF)
    return;
  lua_State *L = lua_getstate();
  lua_rawgeti(L, LUA_REGISTRYINDEX, lua_dataready_ref);
  lua_pushinteger(L, address);
  lua_pushinteger(L, frame[1] >> 4);
  lua_pushinteger(L, rolling_code);
  lua_pushlstring(L, frame, 10);
  luaL_pcallx(L, 4, 0);
}


// ----------------------------------------------------------------------------------------------------//
// ------------------------------- Lua part -----------------------------------------------------------//
// ----------------------------------------------------------------------------------------------------//
static inline void register_lua_cb(lua_State* L, int* cb_ref){
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

int somfy_lua_listen(lua_State* L) { // pin, callback
  NODE_DBG("[somfy_lua_listen]\n");

#if LUA_VERSION_NUM == 501
  if (lua_isnumber(L, 1) && lua_type(L, 2) == LUA_TFUNCTION) {
#else
  if (lua_isinteger(L, 1) && lua_type(L, 2) == LUA_TFUNCTION) {
#endif
    RxPin = luaL_checkinteger(L, 1);
    luaL_argcheck(L, platform_gpio_exists(RxPin) && RxPin>0, 1, "Invalid interrupt pin");
    lua_pushvalue(L, 2);
    register_lua_cb(L, &lua_dataready_ref);

    memset( &SomfyRx, 0, sizeof( SomfyRx) );
    IntBitmask = 1 << pin_num[RxPin];

    MODE_TP1
    NODE_DBG("[somfy_lua_listen] Enabling interrupt on PIN %d\n", RxPin);
    platform_gpio_mode(RxPin, PLATFORM_GPIO_INT, PLATFORM_GPIO_PULLUP);
    NODE_DBG("[somfy_lua_listen] platform_gpio_register_intr_hook - pin: %d, mask: %d\n", RxPin, IntBitmask);
    platform_gpio_register_intr_hook(IntBitmask, InterruptHandler);

    gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[RxPin]), GPIO_PIN_INTR_ANYEDGE);

#if LUA_VERSION_NUM == 501
  } else if ((lua_isnoneornil(L, 1) || lua_isnumber(L, 1)) && lua_isnoneornil(L, 2)) {
#else
  } else if ((lua_isnoneornil(L, 1) || lua_isinteger(L, 1)) && lua_isnoneornil(L, 2)) {
#endif
    NODE_DBG("[somfy_lua_listen] Desabling interrupt on PIN %d\n", RxPin);
    platform_gpio_mode(RxPin, PLATFORM_GPIO_INPUT, PLATFORM_GPIO_PULLUP);

    unregister_lua_cb(L, &lua_dataready_ref);
    RxPin = 0;
  } else {
    luaL_error(L, "Invalid parameters");
  }
  return 0;
}

static void somfy_transmissionDone (task_param_t arg)
{
    lua_State *L = lua_getstate();
    lua_rawgeti (L, LUA_REGISTRYINDEX, lua_done_ref);
    unregister_lua_cb (L, &lua_done_ref);
    luaL_pcallx (L, 0, 0);
}

int somfy_lua_sendcommand(lua_State* L) { // pin, remote, command, rolling_code, num_repeat, callback
    TxPin = luaL_checkinteger(L, 1);
    uint64_t remote = luaL_checkinteger(L, 2);
    uint8_t cmd = luaL_checkinteger(L, 3);
    uint16_t code = luaL_checkinteger(L, 4);
    repeat=luaL_optint( L, 5, 2 );

    luaL_argcheck(L, platform_gpio_exists(TxPin), 1, "Invalid pin");

    if (lua_type(L, 6) == LUA_TFUNCTION) {
      lua_pushvalue (L, 6);
      register_lua_cb (L, &lua_done_ref);
    } else {
      unregister_lua_cb (L, &lua_done_ref);
    }

    MOD_CHECK_ID(gpio, TxPin);
    platform_gpio_mode(TxPin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_PULLUP);

    buildFrame(frame, remote, cmd, code);

    if (!platform_hw_timer_init(TIMER_OWNER, FRC1_SOURCE, TRUE)) {
        // Failed to init the timer
        luaL_error(L, "Unable to initialize timer");
    }
    platform_hw_timer_set_func(TIMER_OWNER, sendCommand, 0);
    sync=2;
    signalindex=0; repeatindex=0;
    sendCommand(0);
    return 0;
}

int luaopen_somfy( lua_State *L ) {
    SendDone_taskid = task_get_id((task_callback_t) somfy_transmissionDone);
    DataReady_taskid = task_get_id((task_callback_t) somfy_decode);
    return 0;
}

// Module function map
LROT_BEGIN(somfy, NULL, 0)
  LROT_FUNCENTRY( sendcommand, somfy_lua_sendcommand )
  LROT_FUNCENTRY( listen, somfy_lua_listen )

  LROT_NUMENTRY( UP, SOMFY_UP )
  LROT_NUMENTRY( DOWN, SOMFY_DOWN )
  LROT_NUMENTRY( PROG, SOMFY_PROG )
  LROT_NUMENTRY( STOP, SOMFY_STOP )
LROT_END(somfy, NULL, 0)

NODEMCU_MODULE(SOMFY, "somfy", somfy, luaopen_somfy);
