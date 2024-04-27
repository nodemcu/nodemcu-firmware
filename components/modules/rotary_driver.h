/*
 * Definitions to access the Rotary driver
 */
#ifndef __ROTARY_H__
#define __ROTARY_H__

#include <stdint.h>

typedef struct {
  uint32_t pos;
  uint32_t time_us;
} rotary_event_t;

struct rotary_driver_handle *rotary_setup(int phaseA,
                                           int phaseB, int press,
                                           task_handle_t tasknumber, void *arg);

bool rotary_getevent(struct rotary_driver_handle *handle, rotary_event_t *result);

bool rotary_has_queued_event(struct rotary_driver_handle *handle);

int rotary_getpos(struct rotary_driver_handle *handle);

int rotary_close(struct rotary_driver_handle *handle);

void rotary_event_handled(struct rotary_driver_handle *handle);

#endif
