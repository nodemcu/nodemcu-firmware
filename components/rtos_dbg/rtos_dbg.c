#include "rtos_dbg.h"
#include <freertos/FreeRTOSConfig.h>
#include <stdio.h>

extern struct {
  uint32_t *pxTopOfStack;
  uint32_t x[5];
  uint32_t y[5];
  unsigned uxPriority;
  uint32_t *pxStack;
  signed char pcTaskName[configMAX_TASK_NAME_LEN];
} *pxCurrentTCB;

void rtos_dbg_task_print (const char *file, uint32_t line)
{
  printf(">>dbg: %s:%lu in RTOS task \"%s\": prio %u\n",
    file,
    line,
    pxCurrentTCB->pcTaskName,
    pxCurrentTCB->uxPriority
  );
}


void rtos_dbg_stack_print (const char *file, uint32_t line)
{
  uint32_t fill = 0xa5a5a5a5u;
  uint32_t nwords = 0;
  uint32_t *p = (uint32_t*)pxCurrentTCB->pxStack;
  for (;p < pxCurrentTCB->pxTopOfStack && *p == fill; ++p)
    ++nwords; 

  printf(">>dbg: %s:%lu in RTOS task \"%s\": %lu stack untouched\n",
    file, line, pxCurrentTCB->pcTaskName, nwords * 4);
}
