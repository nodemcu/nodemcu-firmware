#ifndef _SDK_OVERRIDE_OSAPI_H_
#define _SDK_OVERRIDE_OSAPI_H_

#include "rom.h"
void ets_timer_arm_new (ETSTimer *a, int b, int c, int isMstimer);

#include_next "osapi.h"

#endif
