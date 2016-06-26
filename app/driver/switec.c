/*
 * Module for interfacing with Switec instrument steppers (and
 * similar devices). These are the steppers that are used in automotive 
 * instrument panels and the like. Run off 5 volts at low current.
 *
 * Code inspired by:
 *
 * SwitecX25 Arduino Library
 *  Guy Carpenter, Clearwater Software - 2012
 *
 *  Licensed under the BSD2 license, see license.txt for details.
 *
 * NodeMcu integration by Philip Gladstone, N1DQ
 */

#include "platform.h"
#include "c_types.h"
#include "../libc/c_stdlib.h"
#include "../libc/c_stdio.h"
#include "driver/switec.h"
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "hw_timer.h"
#include "user_interface.h"
#include "task/task.h"

#define N_STATES 6
//
// First pin passed to setup corresponds to bit 3
// On the motor, the pins are arranged
//
//    4           1
//
//    3           2
//
// The direction of rotation can be reversed by reordering the pins
//
// State  3 2 1 0  A B  Value
// 0      1 0 0 1  - -  0x9
// 1      0 0 0 1  . -  0x1
// 2      0 1 1 1  + .  0x7
// 3      0 1 1 0  + +  0x6
// 4      1 1 1 0  . +  0xE
// 5      1 0 0 0  - .  0x8
static const uint8_t stateMap[N_STATES] = {0x9, 0x1, 0x7, 0x6, 0xE, 0x8};

typedef struct {
  uint8_t  current_state;
  uint8_t  stopped;
  int8_t   dir;
  uint32_t mask;
  uint32_t pinstate[N_STATES];
  uint32_t next_time;
  int16_t  target_step;
  int16_t  current_step;
  uint16_t vel;
  uint16_t max_vel;
  uint16_t min_delay;
  task_handle_t task_number;
} DATA;

static DATA *data[SWITEC_CHANNEL_COUNT];
static volatile char timer_active;

#define MAXVEL 255

// Note that this has to be global so that the compiler does not
// put it into ROM.
uint8_t switec_accel_table[][2] = {
    {   20, 3000 >> 4},
    {   50, 1500 >> 4},
    {  100, 1000 >> 4},
    {  150,  800 >> 4},
    {  MAXVEL,  600 >> 4}
};

static void ICACHE_RAM_ATTR timer_interrupt(os_param_t);

#define TIMER_OWNER ((os_param_t) 'S')


// Just takes the channel number
int switec_close(uint32_t channel) 
{
  if (channel >= sizeof(data) / sizeof(data[0])) {
    return -1;
  }

  DATA *d = data[channel];

  if (!d) {
    return 0;
  }

  if (!d->stopped) {
    return -1;
  }

  // Set pins as input
  gpio_output_set(0, 0, 0, d->mask);

  data[channel] = NULL;
  c_free(d);

  // See if there are any other channels active
  for (channel = 0; channel < sizeof(data)/sizeof(data[0]); channel++) {
    if (data[channel]) {
      break;
    }
  }

  // If not, then disable the interrupt
  if (channel >= sizeof(data) / sizeof(data[0])) {
    platform_hw_timer_close(TIMER_OWNER);
  }

  return 0;
}

static __attribute__((always_inline)) inline void write_io(DATA *d) 
{
  uint32_t pin_state = d->pinstate[d->current_state];

  gpio_output_set(pin_state, d->mask & ~pin_state, 0, 0);
}

static __attribute__((always_inline)) inline  void step_up(DATA *d) 
{
  d->current_step++;
  d->current_state = (d->current_state + 1) % N_STATES;
  write_io(d);
}

static __attribute__((always_inline)) inline  void step_down(DATA *d) 
{
  d->current_step--;
  d->current_state = (d->current_state + N_STATES - 1) % N_STATES;
  write_io(d);
}

static void ICACHE_RAM_ATTR timer_interrupt(os_param_t p) 
{
  // This function really is running at interrupt level with everything
  // else masked off. It should take as little time as necessary.
  //
  (void) p;

  int i;
  uint32_t delay = 0xffffffff;

  // Loop over the channels to figure out which one needs action
  for (i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
    DATA *d = data[i];
    if (!d || d->stopped) {
      continue;
    }

    uint32_t now = system_get_time();
    if (now < d->next_time) {
      int need_to_wait = d->next_time - now;
      if (need_to_wait < delay) {
	delay = need_to_wait;
      }
      continue;
    }

    // This channel is past it's action time. Need to process it

    // Are we done yet?
    if (d->current_step == d->target_step && d->vel == 0) {
      d->stopped = 1;
      d->dir = 0;
      task_post_low(d->task_number, 0);
      continue;
    }

    // if stopped, determine direction
    if (d->vel == 0) {
      d->dir = d->current_step < d->target_step ? 1 : -1;
      // do not set to 0 or it could go negative in case 2 below
      d->vel = 1; 
    }
    
    // Move the pointer by one step in the correct direction
    if (d->dir > 0) {
      step_up(d);
    } else {
      step_down(d);
    }

    // determine delta, number of steps in current direction to target.
    // may be negative if we are headed away from target
    int delta = d->dir > 0 ? d->target_step - d->current_step : d->current_step - d->target_step;
    
    if (delta > 0) {
      // case 1 : moving towards target (maybe under accel or decel)
      if (delta <= d->vel) {
	// time to declerate
	d->vel--;
      } else if (d->vel < d->max_vel) {
	// accelerating
	d->vel++;
      } else {
	// at full speed - stay there
      }
    } else {
      // case 2 : at or moving away from target (slow down!)
      d->vel--;
    }
    
    // vel now defines delay
    uint8_t row = 0;
    // this is why vel must not be greater than the last vel in the table.
    while (switec_accel_table[row][0] < d->vel) {
      row++;
    }

    uint32_t micro_delay = switec_accel_table[row][1] << 4;
    if (micro_delay < d->min_delay) {
      micro_delay = d->min_delay;
    }

    // Figure out when we next need to take action
    d->next_time = d->next_time + micro_delay;
    if (d->next_time < now) {
      d->next_time = now + micro_delay;
    }

    // Figure out how long to wait
    int need_to_wait = d->next_time - now;
    if (need_to_wait < delay) {
      delay = need_to_wait;
    }
  } 

  if (delay < 1000000) {
    if (delay < 50) {
      delay = 50;
    }
    timer_active = 1;
    platform_hw_timer_arm_us(TIMER_OWNER, delay);
  } else {
    timer_active = 0;
  }
}


// The pin numbers are actual platform GPIO numbers
int switec_setup(uint32_t channel, int *pin, int max_deg_per_sec, task_handle_t task_number )
{
  if (channel >= sizeof(data) / sizeof(data[0])) {
    return -1;
  }

  if (data[channel]) {
    if (switec_close(channel)) {
      return -1;
    }
  }

  DATA *d = (DATA *) c_zalloc(sizeof(DATA));
  if (!d) {
    return -1;
  }

  if (!data[0] && !data[1] && !data[2]) {
    // We need to stup the timer as no channel was active before
    // no autoreload
    if (!platform_hw_timer_init(TIMER_OWNER, FRC1_SOURCE, FALSE)) {
      // Failed to get the timer
      c_free(d);
      return -1;
    }
  }

  data[channel] = d;
  int i;

  for (i = 0; i < 4; i++) {
    // Build the mask for the pins to be output pins
    d->mask |= 1 << pin[i];

    int j;
    // Now build the hi states for the pins according to the 6 phases above
    for (j = 0; j < N_STATES; j++) {
      if (stateMap[j] & (1 << (3 - i))) {
        d->pinstate[j] |= 1 << pin[i];
      }
    }
  }

  d->max_vel = MAXVEL;
  if (max_deg_per_sec == 0) {
    max_deg_per_sec = 400;
  }
  d->min_delay = 1000000 / (3 * max_deg_per_sec);
  d->task_number = task_number;

#ifdef SWITEC_DEBUG
  for (i = 0; i < 4; i++) {
    c_printf("pin[%d]=%d\n", i, pin[i]);
  }

  c_printf("Mask=0x%x\n", d->mask);
  for (i = 0; i < N_STATES; i++) {
    c_printf("pinstate[%d]=0x%x\n", i, d->pinstate[i]);
  }
#endif

  // Set all pins as outputs
  gpio_output_set(0, 0, d->mask, 0);

  platform_hw_timer_set_func(TIMER_OWNER, timer_interrupt, 0);

  return 0;
}

// All this does is to assert that the current position is 0
int switec_reset(uint32_t channel)
{
  if (channel >= sizeof(data) / sizeof(data[0])) {
    return -1;
  }

  DATA *d = data[channel];

  if (!d || !d->stopped) {
    return -1;
  }

  d->current_step = d->target_step = 0;

  return 0;
}

// Just takes the channel number and the position
int switec_moveto(uint32_t channel, int pos)
{
  if (channel >= sizeof(data) / sizeof(data[0])) {
    return -1;
  }

  DATA *d = data[channel];

  if (!d) {
    return -1;
  }

  if (pos < 0) {
    // This ensures that we don't slam into the endstop
    d->max_vel = 50;
  } else {
    d->max_vel = MAXVEL;
  }

  d->target_step = pos;

  // If the pointer is not moving, setup so that we start it
  if (d->stopped) {
    // reset the timer to avoid possible time overflow giving spurious deltas
    d->next_time = system_get_time() + 1000;
    d->stopped = false;

    if (!timer_active) {
      timer_interrupt(0);
    }
  }

  return 0;  
}

// Get the current position, direction and target position
int switec_getpos(uint32_t channel, int32_t *pos, int32_t *dir, int32_t *target) 
{
  if (channel >= sizeof(data) / sizeof(data[0])) {
    return -1;
  }

  DATA *d = data[channel];

  if (!d) {
    return -1;
  }

  *pos = d->current_step;
  *dir = d->stopped ? 0 : d->dir;
  *target = d->target_step;

  return 0;
}
