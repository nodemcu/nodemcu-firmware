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

#include "os_type.h"
#include "osapi.h"
#include "sections.h"

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "hw_timer.h"
#include "user_interface.h"

#define SYMBOL 640 // symbol width in microseconds
#define SOMFY_UP    0x2
#define SOMFY_STOP  0x1
#define SOMFY_DOWN  0x4
#define SOMFY_PROG  0x8

#define DIRECT_WRITE_LOW(pin)    (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), 0))
#define DIRECT_WRITE_HIGH(pin)   (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), 1))

static const os_param_t TIMER_OWNER = 0x736f6d66; // "somf"
static task_handle_t done_taskid;

static uint8_t    pin;
static uint8_t    frame[7];
static uint8_t    sync;
static uint8_t    repeat;

//static uint32_t   delay[10] = {9415, 89565, 4*SYMBOL, 4*SYMBOL, 4*SYMBOL, 4550, SYMBOL, SYMBOL, SYMBOL, 30415}; // in us
// the `delay` array of constants must be in RAM as it is accessed from the timer interrupt
static const RAM_CONST_SECTION_ATTR uint32_t delay[10] = {US_TO_RTC_TIMER_TICKS(9415), US_TO_RTC_TIMER_TICKS(89565), US_TO_RTC_TIMER_TICKS(4*SYMBOL), US_TO_RTC_TIMER_TICKS(4*SYMBOL), US_TO_RTC_TIMER_TICKS(4*SYMBOL), US_TO_RTC_TIMER_TICKS(4550), US_TO_RTC_TIMER_TICKS(SYMBOL), US_TO_RTC_TIMER_TICKS(SYMBOL), US_TO_RTC_TIMER_TICKS(SYMBOL), US_TO_RTC_TIMER_TICKS(30415)}; // in ticks (no need to recalculate)

static uint8_t    repeatindex;
static uint8_t    signalindex;
static uint8_t    subindex;
static uint8_t    bitcondition;

int lua_done_ref; // callback when transmission is done

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

static void somfy_transmissionDone (task_param_t arg)
{
    lua_State *L = lua_getstate();
    lua_rawgeti (L, LUA_REGISTRYINDEX, lua_done_ref);
    luaL_unref (L, LUA_REGISTRYINDEX, lua_done_ref);
    lua_done_ref = LUA_NOREF;
    lua_call (L, 0, 0);
}

static void ICACHE_RAM_ATTR sendCommand(os_param_t p) {
    (void) p;
    //  NODE_DBG("%d\t%d\n", signalindex, subindex);
    switch (signalindex) {
        case 0:
            subindex = 0;
            if(sync == 2) { // Only with the first frame.
                //Wake-up pulse & Silence
                DIRECT_WRITE_HIGH(pin);
                signalindex++;
                // delayMicroseconds(9415);
                break;
            } else {
                signalindex++; signalindex++; //no break means: go directly to step 3
            }
        case 1:
            //Wake-up pulse & Silence
            DIRECT_WRITE_LOW(pin);
            signalindex++;
            // delayMicroseconds(89565);
            break;
        case 2:
            signalindex++; 
            // no break means go directly to step 3
            // a "useless" step to allow repeating the hardware sync w/o the silence after wake-up pulse
        case 3:
            // Hardware sync: two sync for the first frame, seven for the following ones.
            DIRECT_WRITE_HIGH(pin);
            signalindex++;
            // delayMicroseconds(4*SYMBOL);
            break;
        case 4:
            DIRECT_WRITE_LOW(pin);
            subindex++;
            if (subindex < sync) {signalindex--;} else {signalindex++;}
            // delayMicroseconds(4*SYMBOL);
            break;
        case 5:
            // Software sync
            DIRECT_WRITE_HIGH(pin);
            signalindex++;
            // delayMicroseconds(4550);
            break;
        case 6:
            DIRECT_WRITE_LOW(pin);
            signalindex++;
            subindex=0;
            // delayMicroseconds(SYMBOL);
            break;
        case 7:
            //Data: bits are sent one by one, starting with the MSB.
            bitcondition = ((frame[subindex/8] >> (7 - (subindex%8))) & 1) == 1;
            if(bitcondition) {
                DIRECT_WRITE_LOW(pin);
            }
            else {
                DIRECT_WRITE_HIGH(pin);
            }
            signalindex++;
            // delayMicroseconds(SYMBOL);
            break;
        case 8:
            //Data: bits are sent one by one, starting with the MSB.
            if(bitcondition) {
                DIRECT_WRITE_HIGH(pin);
            }
            else {
                DIRECT_WRITE_LOW(pin);
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
            DIRECT_WRITE_LOW(pin);
            signalindex++;
            // delayMicroseconds(30415); // Inter-frame silence
            break;
        case 10:
            repeatindex++;
            if (repeatindex<repeat) {
                DIRECT_WRITE_HIGH(pin); //start repeat from step 3, but don't wait as after step 1
                signalindex=4; subindex=0; sync=7;
            } else {
                platform_hw_timer_close(TIMER_OWNER);
                if (lua_done_ref != LUA_NOREF) {
                    task_post_low (done_taskid, (task_param_t)0);
                }
            }
            break;
    }
    if (signalindex<10) {
        platform_hw_timer_arm_ticks(TIMER_OWNER, delay[signalindex-1]);
    }
}

static int somfy_lua_sendcommand(lua_State* L) { // pin, remote, command, rolling_code, num_repeat, callback
    if (!lua_isnumber(L, 4)) {
        return luaL_error(L, "wrong arg range");
    }
    pin = luaL_checkinteger(L, 1);
    uint64_t remote = luaL_checkinteger(L, 2);
    uint8_t cmd = luaL_checkinteger(L, 3);
    uint16_t code = luaL_checkinteger(L, 4);
    repeat=luaL_optint( L, 5, 2 );

    luaL_argcheck(L, platform_gpio_exists(pin), 1, "Invalid pin");

    luaL_unref(L, LUA_REGISTRYINDEX, lua_done_ref);
    if (!lua_isnoneornil(L, 6)) {
        lua_pushvalue(L, 6);
        lua_done_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        lua_done_ref = LUA_NOREF;
    }

    MOD_CHECK_ID(gpio, pin);
    platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_PULLUP);

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

static const LUA_REG_TYPE somfy_map[] = {
    { LSTRKEY( "UP" ),    LNUMVAL( SOMFY_UP ) },
    { LSTRKEY( "DOWN" ),    LNUMVAL( SOMFY_DOWN ) },
    { LSTRKEY( "PROG" ),    LNUMVAL( SOMFY_PROG ) },
    { LSTRKEY( "STOP" ),    LNUMVAL( SOMFY_STOP ) },
    { LSTRKEY( "sendcommand" ), LFUNCVAL(somfy_lua_sendcommand)},
    { LNILKEY, LNILVAL}
};

int luaopen_somfy( lua_State *L ) {
    done_taskid = task_get_id((task_callback_t) somfy_transmissionDone);
    return 0;
}

NODEMCU_MODULE(SOMFY, "somfy", somfy_map, luaopen_somfy);