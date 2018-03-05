/*
 * V1.0 Contributed by Jon Lidgard 10/10/2017
 * A partial rewrite of the orginal rfswitch module by Roman Fedorov
 * This version uses the hw_timer interrupt as opposed to the less accurate
 * os_delay_us function of the original.
 *
 * Module for operate 433/315Mhz devices like power outlet sockets, relays, etc.
 * This will most likely work with all popular low cost power outlet sockets
 * with a SC5262 / SC5272, HX2262 / HX2272, PT2262 / PT2272, EV1527,
 * RT1527, FP1527 or HS1527 chipset.
 *
 */

 #include "os_type.h"
 #include "osapi.h"
 #include "sections.h"

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "user_interface.h"
#include "hw_timer.h"


/* A bit is made up of 2 pulses (sub_bits):
 sub_bit flips between 0 & 1 to indicate either the first or 2nd pulses.
 Normally the first pulse is high & the 2nd low unless invertedSignal set.

The following line performs an Xnor function
pin_state = (!m->sub_bit == !m->protocol.invertedSignal)

The following line only decrements signal_index evey 2nd time around
as sub_bit flips between 0 & 1.
m->signal_index = m->signal_index - m->sub_bit
*/

#define DO_PULSE(PULSE_TYPE) do { \
        bool pin_state = (!m->sub_bit == !m->protocol.invertedSignal); \
        GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[m->gpio_pin]), pin_state); \
        delay = m->protocol.PULSE_TYPE[m->sub_bit]; \
        m->signal_index = m->signal_index - m->sub_bit; \
        m->sub_bit = 1 - m->sub_bit; \
 } while(0) // fudge for correct #define expansion (;) in if/else statements


typedef struct Protocol {
  int pulseLength;
  uint8_t syncFactor[2];
  uint8_t zero[2];
  uint8_t one[2];
  /** @brief if true inverts the high and low logic levels */
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
static const RAM_CONST_SECTION_ATTR Protocol proto[] = {
  { 350, {  1, 31 }, {  1,  3 }, {  3,  1 }, false },    // protocol 1
  { 650, {  1, 10 }, {  1,  2 }, {  2,  1 }, false },    // protocol 2
  { 100, { 30, 71 }, {  4, 11 }, {  9,  6 }, false },    // protocol 3
  { 380, {  1,  6 }, {  1,  3 }, {  3,  1 }, false },    // protocol 4
  { 500, {  6, 14 }, {  1,  2 }, {  2,  1 }, false },    // protocol 5
  { 450, { 23,  1 }, {  1,  2 }, {  2,  1 }, true }      // protocol 6 (HT6P20B)
};

typedef struct Message {
  uint8_t repeat_count;
  int8_t signal_index;
  uint16_t pulse_length;
  uint8_t gpio_pin;
  uint8_t signal_length;
  uint8_t sub_bit;
  unsigned long command;
  Protocol protocol;
} Message_t;

static const os_param_t TIMER_OWNER = 0x72667377; // "rfsw"
static Message_t message;

/**
 * Transmit the first 'length' bits of the integer 'code'. The
 * bits are sent from MSB to LSB, i.e., first the bit at position length-1,
 * then the bit at position length-2, and so on, till finally the bit at position 0.
 * Then transmit the sync pulse.
 */


void reset_message(Message_t *m)
{
  m->signal_index=m->signal_length-1;
  m->sub_bit = 0;
}

static void ICACHE_RAM_ATTR send_pulse(os_param_t p) {
    Message_t* m = (Message_t*)p;
    unsigned long delay;

    if (m->signal_index == -1) // Sync Part
    {
        DO_PULSE(syncFactor);
    }
    else // Command Part
    {
        if (m->command & (1L << m->signal_index))  // a '1'
        {
          DO_PULSE(one);
        }
        else // a '0'
        {
          DO_PULSE(zero);
        }
    }

    if (m->signal_index < -1) // finishedTransmit
    {
      m->repeat_count--;
      if (m->repeat_count == 0) // sent all repeats
      {    // we're done!
        platform_hw_timer_close(TIMER_OWNER);
        return;
      }
      else // need to repeat
      {
        reset_message(m);
      }
    }
    // Setup the next interrupt
    platform_hw_timer_arm_ticks(TIMER_OWNER, delay * m->pulse_length);
}


static int rfswitch_send( lua_State *L )
{
  unsigned int protocol_id = luaL_checkinteger( L, 1 );

  message.protocol = proto[protocol_id-1];
  uint16_t pl = luaL_checkinteger( L, 2 );
  if (pl == 0)
  {
    pl = message.protocol.pulseLength;
  }
  // Convert pulse length in us to timer ticks
  message.pulse_length = US_TO_RTC_TIMER_TICKS(pl);
  message.repeat_count = luaL_checkinteger( L, 3 );
  message.gpio_pin = luaL_checkinteger( L, 4 );
  message.command = luaL_checkinteger( L, 5 );
  message.signal_length = luaL_checkinteger( L, 6 );

  reset_message(&message);

  platform_gpio_mode(message.gpio_pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);

  if (!platform_hw_timer_init(TIMER_OWNER, FRC1_SOURCE, FALSE)) {
      // Failed to init the timer
      luaL_error(L, "Unable to initialize timer");
  }
  platform_hw_timer_set_func(TIMER_OWNER, send_pulse, (os_param_t)&message);

  send_pulse((os_param_t)&message);

  return 0;
}

// Module function map
static const LUA_REG_TYPE rfswitch_map[] =
{
  { LSTRKEY( "send" ),       LFUNCVAL( rfswitch_send ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(RFSWITCH, "rfswitch", rfswitch_map, NULL);
