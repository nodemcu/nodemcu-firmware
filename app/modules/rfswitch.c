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

/**
 * Transmit a single high-low pulse.
 */
void transmit(HighLow pulses, bool invertedSignal, int pulseLength, int pin) {
  platform_gpio_write(pin, !invertedSignal);
  os_delay_us(pulseLength * pulses.high);
  platform_gpio_write(pin, invertedSignal);
  os_delay_us(pulseLength * pulses.low);
}

/**
 * Transmit the first 'length' bits of the integer 'code'. The
 * bits are sent from MSB to LSB, i.e., first the bit at position length-1,
 * then the bit at position length-2, and so on, till finally the bit at position 0.
 */
void send(unsigned long protocol_id, unsigned long pulse_length, unsigned long repeat, unsigned long pin, unsigned long value, unsigned int length) {
  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  Protocol p = proto[protocol_id-1];
  for (int nRepeat = 0; nRepeat < repeat; nRepeat++) {
    for (int i = length-1; i >= 0; i--) {
      if (value & (1L << i))
        transmit(p.one, p.invertedSignal, pulse_length, pin);
      else
        transmit(p.zero, p.invertedSignal, pulse_length, pin);
    }
    transmit(p.syncFactor, p.invertedSignal, pulse_length, pin);
  }
}


static int rfswitch_send( lua_State *L )
{
  unsigned int protocol_id = luaL_checkinteger( L, 1 );
  unsigned int pulse_length = luaL_checkinteger( L, 2 );
  unsigned int repeat = luaL_checkinteger( L, 3 );
  unsigned int pin = luaL_checkinteger( L, 4 );
  unsigned long value = luaL_checkinteger( L, 5 );
  unsigned long length = luaL_checkinteger( L, 6 );
  send(protocol_id, pulse_length, repeat, pin, value, length);
  return 0;
}

// Module function map
static const LUA_REG_TYPE rfswitch_map[] =
{
  { LSTRKEY( "send" ),       LFUNCVAL( rfswitch_send ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(RFSWITCH, "rfswitch", rfswitch_map, NULL);
