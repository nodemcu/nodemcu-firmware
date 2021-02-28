#ifndef _TASK_H_
#define _TASK_H_
/*
** The task interface is now part of the core platform interface.
** This header is preserved for backwards compatability only.
*/
#include "platform.h"

#define TASK_PRIORITY_LOW    PLATFORM_TASK_PRIORITY_LOW
#define TASK_PRIORITY_MEDIUM PLATFORM_TASK_PRIORITY_MEDIUM
#define TASK_PRIORITY_HIGH   PLATFORM_TASK_PRIORITY_HIGH

#define task_post(priority,handle,param)  platform_post(priority,handle,param)
#define task_post_low(handle,param)       platform_post_low(handle,param)
#define task_post_medium(handle,param)    platform_post_medium(handle,param)
#define task_post_high(handle,param)      platform_post_high(handle,param)

#define task_handle_t     platform_task_handle_t
#define task_param_t      platform_task_param_t
#define task_callback_t   platform_task_callback_t
#define task_get_id       platform_task_get_id

#endif

