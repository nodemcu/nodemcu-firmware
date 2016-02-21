/*
 * Definitions to access the Switec driver
 */
#ifndef __SWITEC_H__
#define __SWITEC_H__

#include "c_types.h"

#define SWITEC_CHANNEL_COUNT	3

int switec_setup(uint32_t channel, int *pin, int max_deg_per_sec, task_handle_t taskNumber );

int switec_close(uint32_t channel);

int switec_moveto(uint32_t channel, int pos);

int switec_reset(uint32_t channel);

int switec_getpos(uint32_t channel, int32_t *pos, int32_t *dir, int32_t *target);

#endif
