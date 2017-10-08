/*
 * https://github.com/ffedoroff/nodemcu-firmware contributed by Roman Fedorov
 *
 * Module for operate 433/315Mhz devices like power outlet sockets, relays, etc.
 * This will most likely work with all popular low cost power outlet sockets
 * with a SC5262 / SC5272, HX2262 / HX2272, PT2262 / PT2272, EV1527,
 * RT1527, FP1527 or HS1527 chipset.
 *
 * This module using some code from original rc-switch arduino lib
 * https://github.com/sui77/rc-switch/ but unfortunatelly NodeMCU and Arduino
 * are not fully compatable, and it cause for full rewrite rc-switch lib into new rfswitch lib.
 */

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "user_interface.h"
#include "hw_timer.h"

#define DIRECT_WRITE_LOW(pin)    (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), protocol.invertedSignal))
#define DIRECT_WRITE_HIGH(pin)   (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), !protocol.invertedSignal))

typedef struct HighLow {
  uint8_t high;
  uint8_t low;
} HighLow;

typedef struct Protocol {
  int pulseLength;
  HighLow syncFactor;
  HighLow zero;
  HighLow one;
  /** @brief if true inverts the high and low logic levels in the HighLow structs */
  bool invertedSignal;
} Protocol;

/* Format for protocol definitions:
 * {pulselength, Sync bit, "0" bit, "1" bit}
 *
 * pulselength: pulse length in microseconds, e.g. 350
 * Sync bit: {1, 31} means 1 high pulse and 31 low pulses
 *     (perceived as a 31*pulselength long pulse, total length of sync bit is
 *     32*pulselength microseconds), i.e:
 *      _
 *     | |_______________________________ (don't count the vertical bars)
 * "0" bit: waveform for a data bit of value "0", {1, 3} means 1 high pulse
 *     and 3 low pulses, total length (1+3)*pulselength, i.e:
 *      _
 *     | |___
 * "1" bit: waveform for a data bit of value "1", e.g. {3,1}:
 *      ___
 *     |   |_
 *
 * These are combined to form Tri-State bits when sending or receiving codes.
 */
static const Protocol proto[] = {
  { 350, {  1, 31 }, {  1,  3 }, {  3,  1 }, false },    // protocol 1
  { 650, {  1, 10 }, {  1,  2 }, {  2,  1 }, false },    // protocol 2
  { 100, { 30, 71 }, {  4, 11 }, {  9,  6 }, false },    // protocol 3
  { 380, {  1,  6 }, {  1,  3 }, {  3,  1 }, false },    // protocol 4
  { 500, {  6, 14 }, {  1,  2 }, {  2,  1 }, false },    // protocol 5
  { 450, { 23,  1 }, {  1,  2 }, {  2,  1 }, true }      // protocol 6 (HT6P20B)
};

static const os_param_t TIMER_OWNER = 0x7266; // "rf"

static uint8_t    repeatindex;
static uint8_t    signalindex;
static uint8_t pulse_part;
static uint8_t pulse_length;
static uint8_t pin;
static bool do_sync;
static unsigned long signal_length;
static unsigned long command;
static unsigned int length;

Protocol protocol;

/**
 * Transmit the first 'length' bits of the integer 'code'. The
 * bits are sent from MSB to LSB, i.e., first the bit at position length-1,
 * then the bit at position length-2, and so on, till finally the bit at position 0.
 */

static void prepare_to_send()
{
  signalindex=signal_length-1;
  pulse_part=0;
  do_sync = 0;
}

static void ICACHE_RAM_ATTR send_pulse(os_param_t p) {
    (void) p;
    unsigned long delay;
    if (do_sync)
    {
      if (pulse_part == 0)
      {
        DIRECT_WRITE_HIGH(pin);
        pulse_part=1;
        delay = protocol.syncFactor.high;
      } else
      {
        DIRECT_WRITE_LOW(pin);
        do_sync=false;
        delay = protocol.syncFactor.low;
      }
    }
    else
    {
      if (command & (1L << signalindex))  // a '1'
      {
        if (pulse_part == 0)
        {
          DIRECT_WRITE_HIGH(pin);
          pulse_part=1;
          delay = protocol.one.high;
        }
        else
        {
          DIRECT_WRITE_LOW(pin);
          pulse_part=0;
          signalindex--;
          delay = protocol.one.low;
        }
      }
      else // a '0'
      {
        if (pulse_part == 0)
        {
          DIRECT_WRITE_HIGH(pin);
          pulse_part=1;
          delay = protocol.zero.high;
        }
        else
        {
          DIRECT_WRITE_LOW(pin);
          pulse_part=0;
          signalindex--;
          delay = protocol.zero.low;
        }
      }
      if (signalindex == 0)
      {
        do_sync = true;
      }
    }

    bool finishedTransmit = (signalindex == 0) && !do_sync;
    if (finishedTransmit)
    {
      repeatindex--;
      if (repeatindex == 0) // sent all repeats
      {
        platform_hw_timer_close(TIMER_OWNER);
        // we're done!
      }
      else // need to repeat
      {
        prepare_to_send();
        send_pulse(0);
      }
    }
    else // re-arm the timer for the next pulse part
    {
      platform_hw_timer_arm_ticks(TIMER_OWNER, delay * pulse_length);
    }
}

static int rfswitch_send( lua_State *L )
{
  unsigned int protocol_id = luaL_checkinteger( L, 1 );
  pulse_length = luaL_checkinteger( L, 2 );
  repeatindex = luaL_checkinteger( L, 3 );
  pin = luaL_checkinteger( L, 4 );
  command = luaL_checkinteger( L, 5 );
  signal_length = luaL_checkinteger( L, 6 );

  if (!platform_hw_timer_init(TIMER_OWNER, FRC1_SOURCE, TRUE)) {
      // Failed to init the timer
      luaL_error(L, "Unable to initialize timer");
  }
  platform_hw_timer_set_func(TIMER_OWNER, send_pulse, 0);

  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  protocol = proto[protocol_id-1];

  prepare_to_send();
  send_pulse(0);
  return 0;
}

// Module function map
static const LUA_REG_TYPE rfswitch_map[] =
{
  { LSTRKEY( "send" ),       LFUNCVAL( rfswitch_send ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(RFSWITCH, "rfswitch", rfswitch_map, NULL);
