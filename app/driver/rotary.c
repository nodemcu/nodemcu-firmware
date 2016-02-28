/*
 * Driver for interfacing to cheap rotary switches that
 * have a quadrature output with an optional press button
 *
 * This sets up the relevant gpio as interrupt and then keeps track of
 * the position of the switch in software. Changes are enqueued to task
 * level and a task message posted when required. If the queue fills up
 * then moves are ignored, but the last press/release will be included.
 *
 * Philip Gladstone, N1DQ
 */

#include "platform.h"
#include "c_types.h"
#include "../libc/c_stdlib.h"
#include "../libc/c_stdio.h"
#include "driver/rotary.h"
#include "user_interface.h"
#include "task/task.h"
#include "ets_sys.h"

//
//  Queue is empty if read == write. 
//  However, we always want to keep the previous value
//  so writing is only allowed if write - read < QUEUE_SIZE - 1

#define QUEUE_SIZE 	8

#define GET_LAST_STATUS(d)	(d->queue[(d->write_offset-1) & (QUEUE_SIZE - 1)])
#define GET_PREV_STATUS(d)	(d->queue[(d->write_offset-2) & (QUEUE_SIZE - 1)])
#define HAS_QUEUED_DATA(d)	(d->read_offset < d->write_offset)
#define HAS_QUEUE_SPACE(d)	(d->read_offset + QUEUE_SIZE - 1 > d->write_offset)

#define REPLACE_STATUS(d, x)    (d->queue[(d->write_offset-1) & (QUEUE_SIZE - 1)] = (rotary_event_t) { (x), system_get_time() })
#define QUEUE_STATUS(d, x)      (d->queue[(d->write_offset++) & (QUEUE_SIZE - 1)] = (rotary_event_t) { (x), system_get_time() })

#define GET_READ_STATUS(d)	(d->queue[d->read_offset & (QUEUE_SIZE - 1)])
#define ADVANCE_IF_POSSIBLE(d)  if (d->read_offset < d->write_offset) { d->read_offset++; }

#define STATUS_IS_PRESSED(x)	((x & 0x80000000) != 0)

typedef struct {
  int8_t   phase_a_pin;
  int8_t   phase_b_pin;
  int8_t   press_pin;
  uint32_t read_offset;  // Accessed by task
  uint32_t write_offset;	// Accessed by ISR
  uint32_t pin_mask;
  uint32_t phase_a;
  uint32_t phase_b;
  uint32_t press;
  uint32_t last_press_change_time;
  int	   tasknumber;
  rotary_event_t    queue[QUEUE_SIZE];
} DATA;

static DATA *data[ROTARY_CHANNEL_COUNT];

static uint8_t task_queued;

static void set_gpio_bits(void);

static void rotary_clear_pin(int pin) 
{
  if (pin >= 0) {
    gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[pin]), GPIO_PIN_INTR_DISABLE);
    platform_gpio_mode(pin, PLATFORM_GPIO_INPUT, PLATFORM_GPIO_PULLUP);
  }
}

// Just takes the channel number. Cleans up the resources used.
int rotary_close(uint32_t channel) 
{
  if (channel >= sizeof(data) / sizeof(data[0])) {
    return -1;
  }

  DATA *d = data[channel];

  if (!d) {
    return 0;
  }

  data[channel] = NULL;

  rotary_clear_pin(d->phase_a_pin);
  rotary_clear_pin(d->phase_b_pin);
  rotary_clear_pin(d->press_pin);

  c_free(d);

  set_gpio_bits();

  return 0;
}

static uint32_t  ICACHE_RAM_ATTR rotary_interrupt(uint32_t ret_gpio_status) 
{
  // This function really is running at interrupt level with everything
  // else masked off. It should take as little time as necessary.
  //
  //

  // This gets the set of pins which have changed status
  uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);

  int i;
  for (i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
    DATA *d = data[i];
    if (!d || (gpio_status & d->pin_mask) == 0) {
      continue;
    }

    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & d->pin_mask);

    uint32_t bits = GPIO_REG_READ(GPIO_IN_ADDRESS);

    uint32_t last_status = GET_LAST_STATUS(d).pos;

    uint32_t now = system_get_time();

    uint32_t new_status;

    new_status = last_status & 0x80000000;

    // This is the debounce logic for the press switch. We ignore changes
    // for 10ms after a change.
    if (now - d->last_press_change_time > 10 * 1000) {
      new_status = (bits & d->press) ? 0 : 0x80000000;
      if (STATUS_IS_PRESSED(new_status ^ last_status)) {
        d->last_press_change_time = now;
      }
    }

    //  A   B
    //  1   1   => 0
    //  1   0   => 1
    //  0   0   => 2
    //  0   1   => 3

    int micropos = 2;
    if (bits & d->phase_b) {
      micropos = 3;
    }
    if (bits & d->phase_a) {
      micropos ^= 3;
    }

    int32_t rotary_pos = last_status;

    switch ((micropos - last_status) & 3) {
      case 0:
        // No change, nothing to do
	break;
      case 1:
        // Incremented by 1
	rotary_pos++;
	break;
      case 3:
        // Decremented by 1
	rotary_pos--;
	break;
      default:
        // We missed an interrupt
	// We will ignore... but mark it.
	rotary_pos += 1000000;
	break;
    }

    new_status |= rotary_pos & 0x7fffffff;
    
    if (last_status != new_status) {
      // Either we overwrite the status or we add a new one
      if (!HAS_QUEUED_DATA(d) 
	  || STATUS_IS_PRESSED(last_status ^ new_status)
	  || STATUS_IS_PRESSED(last_status ^ GET_PREV_STATUS(d).pos)) {
	if (HAS_QUEUE_SPACE(d)) {
	  QUEUE_STATUS(d, new_status);
	  if (!task_queued) {
	    if (task_post_medium(d->tasknumber, (os_param_t) &task_queued)) {
	      task_queued = 1;
	    }
	  }
	} else {
	  REPLACE_STATUS(d, new_status);
	}
      } else {
	REPLACE_STATUS(d, new_status);
      }
    }
    ret_gpio_status &= ~(d->pin_mask);
  }

  return ret_gpio_status;
}

// The pin numbers are actual platform GPIO numbers
int rotary_setup(uint32_t channel, int phase_a, int phase_b, int press, task_handle_t tasknumber )
{
  if (channel >= sizeof(data) / sizeof(data[0])) {
    return -1;
  }

  if (data[channel]) {
    if (rotary_close(channel)) {
      return -1;
    }
  }

  DATA *d = (DATA *) c_zalloc(sizeof(DATA));
  if (!d) {
    return -1;
  }

  data[channel] = d;
  int i;

  d->tasknumber = tasknumber;

  d->phase_a = 1 << pin_num[phase_a];
  platform_gpio_mode(phase_a, PLATFORM_GPIO_INT, PLATFORM_GPIO_PULLUP);
  gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[phase_a]), GPIO_PIN_INTR_ANYEDGE);
  d->phase_a_pin = phase_a;

  d->phase_b = 1 << pin_num[phase_b];
  platform_gpio_mode(phase_b, PLATFORM_GPIO_INT, PLATFORM_GPIO_PULLUP);
  gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[phase_b]), GPIO_PIN_INTR_ANYEDGE);
  d->phase_b_pin = phase_b;

  if (press >= 0) {
    d->press = 1 << pin_num[press];
    platform_gpio_mode(press, PLATFORM_GPIO_INT, PLATFORM_GPIO_PULLUP);
    gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[press]), GPIO_PIN_INTR_ANYEDGE);
  }
  d->press_pin = press;

  d->pin_mask = d->phase_a | d->phase_b | d->press;

  set_gpio_bits();

  return 0;
}

static void set_gpio_bits()
{
  uint32_t bits = 0;
  for (int i = 0; i < ROTARY_CHANNEL_COUNT; i++) {
    DATA *d = data[i];

    if (d) {
      bits = bits | d->pin_mask;
    }
  }

  platform_gpio_register_intr_hook(bits, rotary_interrupt);
}

bool rotary_has_queued_event(uint32_t channel)
{
  if (channel >= sizeof(data) / sizeof(data[0])) {
    return FALSE;
  }

  DATA *d = data[channel];

  if (!d) {
    return FALSE;
  }

  return HAS_QUEUED_DATA(d);
}

// Get the oldest event in the queue and remove it (if possible)
bool rotary_getevent(uint32_t channel, rotary_event_t *resultp) 
{
  rotary_event_t result = { 0 };
  
  if (channel >= sizeof(data) / sizeof(data[0])) {
    return FALSE;
  }

  DATA *d = data[channel];

  if (!d) {
    return FALSE;
  }

  ETS_GPIO_INTR_DISABLE();

  bool status = FALSE;

  if (HAS_QUEUED_DATA(d)) {
    result = GET_READ_STATUS(d);
    d->read_offset++;
    status = TRUE;
  } else {
    result = GET_LAST_STATUS(d);
  }

  ETS_GPIO_INTR_ENABLE();

  *resultp = result;

  return status;
}

int rotary_getpos(uint32_t channel)
{
  if (channel >= sizeof(data) / sizeof(data[0])) {
    return -1;
  }

  DATA *d = data[channel];

  if (!d) {
    return -1;
  }

  return GET_LAST_STATUS(d).pos;
}
