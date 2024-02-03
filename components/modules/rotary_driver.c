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
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "task/task.h"
#include "rotary_driver.h"
#include "driver/gpio.h"
#include "esp_timer.h"


//
//  Queue is empty if read == write.
//  However, we always want to keep the previous value
//  so writing is only allowed if write - read < QUEUE_SIZE - 1

#define QUEUE_SIZE 	8

#define GET_LAST_STATUS(d)	(d->queue[(d->write_offset-1) & (QUEUE_SIZE - 1)])
#define GET_PREV_STATUS(d)	(d->queue[(d->write_offset-2) & (QUEUE_SIZE - 1)])
#define HAS_QUEUED_DATA(d)	(d->read_offset < d->write_offset)
#define HAS_QUEUE_SPACE(d)	(d->read_offset + QUEUE_SIZE - 1 > d->write_offset)

#define REPLACE_STATUS(d, x)    (d->queue[(d->write_offset-1) & (QUEUE_SIZE - 1)] = (rotary_event_t) { (x), esp_timer_get_time() })
#define QUEUE_STATUS(d, x)      (d->queue[(d->write_offset++) & (QUEUE_SIZE - 1)] = (rotary_event_t) { (x), esp_timer_get_time() })

#define GET_READ_STATUS(d)	(d->queue[d->read_offset & (QUEUE_SIZE - 1)])
#define ADVANCE_IF_POSSIBLE(d)  if (d->read_offset < d->write_offset) { d->read_offset++; }

#define STATUS_IS_PRESSED(x)	(((x) & 0x80000000) != 0)

typedef struct {
  int8_t   phase_a_pin;
  int8_t   phase_b_pin;
  int8_t   press_pin;
  uint32_t read_offset;  // Accessed by task
  uint32_t write_offset;	// Accessed by ISR
  uint32_t last_press_change_time;
  int	   tasknumber;
  rotary_event_t    queue[QUEUE_SIZE];
} DATA;

static DATA *data[ROTARY_CHANNEL_COUNT];

static uint8_t task_queued;

static void set_gpio_mode(int pin, gpio_int_type_t intr)
{
  gpio_config_t config = {
    .pin_bit_mask = 1LL << pin,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .intr_type = intr
  };

  gpio_config(&config);
}

static void rotary_clear_pin(int pin)
{
  if (pin >= 0) {
    gpio_isr_handler_remove(pin);
    set_gpio_mode(pin, GPIO_INTR_DISABLE);
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

  free(d);

  return 0;
}

static void rotary_interrupt(void *arg)
{
  // This function runs with high priority
  DATA *d = (DATA *) arg;

  uint32_t last_status = GET_LAST_STATUS(d).pos;

  uint32_t now = esp_timer_get_time();

  uint32_t new_status;

  new_status = last_status & 0x80000000;

  // This is the debounce logic for the press switch. We ignore changes
  // for 10ms after a change.
  if (now - d->last_press_change_time > 10 * 1000) {
    new_status = gpio_get_level(d->press_pin) ? 0 : 0x80000000;
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
  if (gpio_get_level(d->phase_b_pin)) {
    micropos = 3;
  }
  if (gpio_get_level(d->phase_a_pin)) {
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
          if (task_post_medium(d->tasknumber, (task_param_t) &task_queued)) {
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

  DATA *d = (DATA *) calloc(1, sizeof(DATA));
  if (!d) {
    return -1;
  }

  data[channel] = d;

  d->tasknumber = tasknumber;

  set_gpio_mode(phase_a, GPIO_INTR_ANYEDGE);
  gpio_isr_handler_add(phase_a, rotary_interrupt, d);
  d->phase_a_pin = phase_a;

  set_gpio_mode(phase_b, GPIO_INTR_ANYEDGE);
  gpio_isr_handler_add(phase_b, rotary_interrupt, d);
  d->phase_b_pin = phase_b;

  if (press >= 0) {
    set_gpio_mode(press, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(press, rotary_interrupt, d);
  }
  d->press_pin = press;

  return 0;
}

bool rotary_has_queued_event(uint32_t channel)
{
  if (channel >= sizeof(data) / sizeof(data[0])) {
    return false;
  }

  DATA *d = data[channel];

  if (!d) {
    return false;
  }

  return HAS_QUEUED_DATA(d);
}

// Get the oldest event in the queue and remove it (if possible)
bool rotary_getevent(uint32_t channel, rotary_event_t *resultp)
{
  rotary_event_t result = { 0 };

  if (channel >= sizeof(data) / sizeof(data[0])) {
    return false;
  }

  DATA *d = data[channel];

  if (!d) {
    return false;
  }

  bool status = false;

  if (HAS_QUEUED_DATA(d)) {
    result = GET_READ_STATUS(d);
    d->read_offset++;
    status = true;
  } else {
    result = GET_LAST_STATUS(d);
  }

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

esp_err_t rotary_driver_init()
{
  return gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
}
