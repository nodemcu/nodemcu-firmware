#ifndef _RTOS_DBG_H_
#define _RTOS_DBG_H_

#include <stdint.h>

#define rtos_dbg_here() do{rtos_dbg_task_print(__FILE__,__LINE__);} while(0)
#define rtos_dbg_stack_here() do{rtos_dbg_stack_print(__FILE__,__LINE__);} while(0)

void rtos_dbg_task_print (const char *file, uint32_t line);
void rtos_dbg_stack_print (const char *file, uint32_t line);

#endif
