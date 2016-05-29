/*
 * Definitions to access the Rotary driver
 */
#ifndef __ROTARY_H__
#define __ROTARY_H__

#include "c_types.h"

#define ROTARY_CHANNEL_COUNT	3

typedef struct {
  uint32_t pos;
  uint32_t time_us;
} rotary_event_t;

int rotary_setup(uint32_t channel, int phaseA, int phaseB, int press, task_handle_t tasknumber);

bool rotary_getevent(uint32_t channel, rotary_event_t *result);

bool rotary_has_queued_event(uint32_t channel);

int rotary_getpos(uint32_t channel);

int rotary_close(uint32_t channel);

#endif
