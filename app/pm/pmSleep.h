#ifndef __FPM_SLEEP_H__
#define __FPM_SLEEP_H__
#include "user_interface.h"
#include "c_types.h"
#include "lauxlib.h"
#include "gpio.h"
#include "platform.h"
#include "task/task.h"
#include "c_string.h"

#if defined(DEVELOP_VERSION)
#define PMSLEEP_DEBUG
#endif

#if defined(PMSLEEP_DEBUG)
  #define PMSLEEP_DBG(fmt, ...) dbg_printf("\tPMSLEEP(%s):"fmt"\n", __FUNCTION__, ##__VA_ARGS__)
#else
  #define PMSLEEP_DBG(...) //c_printf(__VA_ARGS__)
#endif

#if defined(NODE_ERROR)
  #define PMSLEEP_ERR(fmt, ...) NODE_ERR("%s"fmt"\n", "PMSLEEP:", ##__VA_ARGS__)
#else
  #define PMSLEEP_ERR(...)
#endif





#define PMSLEEP_SLEEP_MIN_TIME 50000
#define PMSLEEP_SLEEP_MAX_TIME 268435454 //FPM_MAX_SLEEP_TIME-1
#define pmSleep_INIT_CFG(X) pmSleep_param_t X = {.sleep_duration=0, .wake_pin=255, \
    .preserve_opmode=TRUE, .suspend_cb_ptr=NULL, .resume_cb_ptr=NULL}

#define PMSLEEP_INT_MAP \
  { LSTRKEY( "INT_BOTH" ),      LNUMVAL( GPIO_PIN_INTR_ANYEDGE ) }, \
  { LSTRKEY( "INT_UP" ),        LNUMVAL( GPIO_PIN_INTR_POSEDGE ) }, \
  { LSTRKEY( "INT_DOWN" ),      LNUMVAL( GPIO_PIN_INTR_NEGEDGE ) }, \
  { LSTRKEY( "INT_HIGH" ),      LNUMVAL( GPIO_PIN_INTR_HILEVEL ) }, \
  { LSTRKEY( "INT_LOW" ),       LNUMVAL( GPIO_PIN_INTR_LOLEVEL ) }



typedef struct pmSleep_param{
  uint32 sleep_duration;
  uint8 sleep_mode;
  uint8 wake_pin;
  uint8 int_type;
  bool preserve_opmode;
  void (*suspend_cb_ptr)(void);
  void (*resume_cb_ptr)(void);
}pmSleep_param_t; //structure to hold pmSleep configuration


enum PMSLEEP_STATE{
  PMSLEEP_AWAKE = 0,
  PMSLEEP_SUSPENSION_PENDING = 1,
  PMSLEEP_SUSPENDED = 2
};

uint8 pmSleep_get_state(void);
void pmSleep_resume(void (*resume_cb_ptr)(void));
void pmSleep_suspend(pmSleep_param_t *param);
void pmSleep_execute_lua_cb(int* cb_ref);
int pmSleep_parse_table_lua( lua_State* L, int table_idx, pmSleep_param_t *cfg, int *suspend_lua_cb_ref, int *resume_lua_cb_ref);


#endif // __FPM_SLEEP_H__
