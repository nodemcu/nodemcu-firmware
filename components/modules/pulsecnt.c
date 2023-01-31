/* 
Pulse counter module for ESP32 to allow interfacing from Lua
Authored by: ChiliPeppr (John Lauer) 2019

ESP-IDF docs for Pulse Counter
https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/pcnt.html

This example code is in the Public Domain (or CC0 licensed, at your option.)
Make modifications at will and freely.

Unless required by applicable law or agreed to in writing, this
software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.
*/

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "task/task.h"
#include "driver/pcnt.h"
#include "soc/pcnt_struct.h"
#include "esp_log.h"
#include "lextra.h"

#include <string.h>


pcnt_isr_handle_t user_isr_handle = NULL; //user's ISR service handle

/* A sample structure to pass events from the PCNT
 * interrupt handler to the main program.
 */
typedef struct {
    uint8_t unit;  // the PCNT unit that originated an interrupt
    uint32_t status; // information on the event type that caused the interrupt
} pcnt_evt_t;

typedef struct{
  // PulsecntHandle_t pcnt;
  int32_t cb_ref, self_ref;
  uint8_t unit; //=0, -- Defaults to 0. ESP32 has 0 thru 7 units to count pulses on
  bool is_initted;
  bool is_debug;
  int ch0_pulse_gpio_num; // needs to be signed to support PCNT_PIN_NOT_USED of -1
  int ch0_ctrl_gpio_num; // needs to be signed to support PCNT_PIN_NOT_USED of -1
  bool ch0_is_defined;
  uint8_t ch0_pos_mode;
  uint8_t ch0_neg_mode;
  uint8_t ch0_lctrl_mode;
  uint8_t ch0_hctrl_mode;
  int16_t ch0_counter_l_lim;
  int16_t ch0_counter_h_lim;
  bool ch1_is_defined;
  int ch1_pulse_gpio_num; // needs to be signed to support PCNT_PIN_NOT_USED of -1
  int ch1_ctrl_gpio_num; // needs to be signed to support PCNT_PIN_NOT_USED of -1
  uint8_t ch1_pos_mode;
  uint8_t ch1_neg_mode;
  uint8_t ch1_lctrl_mode;
  uint8_t ch1_hctrl_mode;
  int16_t ch1_counter_l_lim;
  int16_t ch1_counter_h_lim;
  int16_t thresh0;  // thresh0 is for the unit, not the channel
  int16_t thresh1;  // thresh1 is for the unit, not the channel
  uint32_t counter;
} pulsecnt_struct_t;
typedef pulsecnt_struct_t *pulsecnt_t;

// array of 8 pulsecnt_struct_t pointers so we can reference by unit number
// this array gets filled in as we define pulsecnt_struct_t's during the create() method
static pulsecnt_t pulsecnt_selfs[8];

// Task ID to get ISR interrupt back into Lua callback
static task_handle_t pulsecnt_task_id;

/* Decode what PCNT's unit originated an interrupt
 * and pass this information together with the event type
 * the main program.
 */
static void IRAM_ATTR pulsecnt_intr_handler(void *arg)
{
    uint32_t intr_status = PCNT.int_st.val;
    uint8_t i;
    pcnt_evt_t evt;
    // portBASE_TYPE HPTaskAwoken = pdFALSE;

    for (i = 0; i < 8; i++) {
        if (intr_status & (BIT(i))) {
            evt.unit = i;
            /* Save the PCNT event type that caused an interrupt
               to pass it to the main program */
            evt.status = PCNT.status_unit[i].val;
            PCNT.int_clr.val = BIT(i);
            

            // post using lua task posting technique
            // on lua_open we set pulsecnt_task_id as a method which gets called
            // by Lua after task_post_high with reference to this self object and then we can steal the 
            // callback_ref and then it gets called by lua_call where we get to add our args
            task_post_isr_high(pulsecnt_task_id, (evt.status << 8) | evt.unit );
        }
    }
}

/*
This method gets called from the IRAM interuppt method via Lua's task queue. That lets the interrupt 
run clean while this method gets called at a lower priority to not break the IRAM interrupt high priority.
We will do the actual callback here for the user with the fully decoded state of the pulse count.
The format of the callback to your Lua code is:
  function onPulseCnt(unit, isThr0, isThr1, isLLim, isHLim, isZero)
*/
static void pulsecnt_task(task_param_t param, task_prio_t prio)
{
  (void)prio;

  // we bit packed the unit number and status into 1 int in the IRAM interrupt so need to unpack here
  uint8_t unit = (uint8_t)param & 0xffu;
  int status = ((uint32_t)param >> 8);
  
  // int16_t cur_count, evt_count = 0;
  // pcnt_get_counter_value(unit, &cur_count);

  // try to get the pulsecnt_struct_t from the pulsecnt_selfs array 
  pulsecnt_t pc = pulsecnt_selfs[unit];
  if (pc->is_debug) ESP_LOGI("pulsecnt", "Cb for unit %d, gpio: %d, ctrl_gpio: %d, pos_mode: %d, neg_mode: %d, lctrl_mode: %d, hctrl_mode: %d, counter_l_lim: %d, counter_h_lim: %d", pc->unit, pc->ch0_pulse_gpio_num, pc->ch0_ctrl_gpio_num, pc->ch0_pos_mode, pc->ch0_neg_mode, pc->ch0_lctrl_mode, pc->ch0_hctrl_mode, pc->ch0_counter_l_lim, pc->ch0_counter_h_lim );


  bool thr1 = false;
  bool thr0 = false;
  bool l_lim = false;
  bool h_lim = false;
  bool zero = false;
  // char evt_str[20];  // "-32768 or 32768" is 15 chars long and is the max string len
  // bool is_multi_lim = false;

  /*0: positive value to zero; 1: negative value to zero; 2: counter value negative ; 3: counter value positive*/
  // uint8_t moving_to = status & 0x00000003u; // get first two bits

  if (status & PCNT_EVT_THRES_1) {
      // printf("THRES1 EVT\n");
      thr1 = true;
      // evt_count = pc->thresh1;
  }
  if (status & PCNT_EVT_THRES_0) {
      // printf("THRES0 EVT\n");
      thr0 = true;
      // evt_count = pc->thresh0;
  }
  if (status & PCNT_EVT_L_LIM) {
      // printf("L_LIM EVT\n");
      l_lim = true;
      /*
      // see if there is a ch0 and ch1 limit. if so then pass back string. otherwise pass back just one int.
      if (pc->ch0_is_defined && pc->ch1_is_defined) {
        // we need to pass back both because it's indeterminate which limit triggered this and there is 
        // no way to know from the ESP32 API 
        is_multi_lim = true;
        sprintf(evt_str, "%d or %d", pc->ch0_counter_l_lim, pc->ch1_counter_l_lim);
      } else if (pc->ch0_is_defined) {
        // we have a ch0 item, so use its val
        evt_count = pc->ch0_counter_l_lim;
      } else if (pc->ch1_is_defined) {
        // we have a ch1 item, so use its val
        evt_count = pc->ch1_counter_l_lim;
      }
      */
  }
  if (status & PCNT_EVT_H_LIM) {
      // printf("H_LIM EVT\n");
      h_lim = true;
      /*
      // see if there is a ch0 and ch1 limit. if so then pass back string. otherwise pass back just one int.
      if (pc->ch0_is_defined && pc->ch1_is_defined) {
        // we need to pass back both because it's indeterminate which limit triggered this and there is 
        // no way to know from the ESP32 API 
        is_multi_lim = true;
        sprintf(evt_str, "%d or %d", pc->ch0_counter_h_lim, pc->ch1_counter_h_lim);
      } else if (pc->ch0_is_defined) {
        // we have a ch0 item, so use its val
        evt_count = pc->ch0_counter_h_lim;
      } else if (pc->ch1_is_defined) {
        // we have a ch1 item, so use its val
        evt_count = pc->ch1_counter_h_lim;
      }
      */
  }
  if (status & PCNT_EVT_ZERO) {
      // printf("ZERO EVT\n");
      zero = true;
      // evt_count = 0;
  }

  // at the start of turning on the pulse counter you get a stat=255 which is like a 
  // 1st time callback saying it's alive 
  // if (status == 255) {
  //   evt_count = -1;
  // }

  lua_State *L = lua_getstate ();
  if (pc->cb_ref != LUA_NOREF)
  {
    // lua_rawgeti (L, LUA_REGISTRYINDEX, pulsecnt_cb_refs[unit]);
    lua_rawgeti (L, LUA_REGISTRYINDEX, pc->cb_ref);
    lua_pushinteger (L, unit);
    // if (is_multi_lim) {
    //   lua_pushstring(L, evt_str);
    // } else {
    //   lua_pushinteger (L, evt_count);
    // }
    // lua_pushinteger (L, cur_count);
    lua_pushboolean (L, thr0);
    lua_pushboolean (L, thr1);
    lua_pushboolean (L, l_lim);
    lua_pushboolean (L, h_lim);
    lua_pushboolean (L, zero);
    // lua_pushinteger (L, moving_to);
    // lua_pushinteger (L, status);
    luaL_pcallx (L, 6, 0);
  } else {
    if (pc->is_debug) ESP_LOGI("pulsecnt", "Could not find cb for unit %d with ptr %ld", unit, pc->cb_ref);
  }
}

// Get the pulsecnt.pctr object from the stack which is the struct pulsecnt_t
static pulsecnt_t pulsecnt_get( lua_State *L, int stack )
{
  return (pulsecnt_t)luaL_checkudata(L, stack, "pulsecnt.pctr");
}

// Lua: pc:setFilter(clkCyclesToIgnore)
// Example: pc:setFilter(100) -- Ignore any signal shorter than 100 clock cycles. 80Mhz clock.
// You can ignore from 0 to 1023 clock cycles
static int pulsecnt_set_filter( lua_State *L ) {
  int stack = 0;

  // when we're called from an object the stack index 1 has our self ref 
  pulsecnt_t pc = pulsecnt_get(L, ++stack);

  // Get clkCyclesToIgnore -- first arg after self arg
  int clkCyclesToIgnore = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, clkCyclesToIgnore >= 0 && clkCyclesToIgnore <= 1023, stack, "The clkCyclesToIgnore number allows 0 to 1023");

  pcnt_set_filter_value(pc->unit, clkCyclesToIgnore);

  if (pc->is_debug) ESP_LOGI("pulsecnt", "Setup filter for unit %d with clkCyclesToIgnore %d", pc->unit, clkCyclesToIgnore);

  return 0;

}

// Lua: pc:setThres(thresh0_val, thresh1_val)
// Example: pc:setThres(-5, 5)
// When you set the threshold, the pulse counter will be reset and the callback will be attached.
static int pulsecnt_set_thres( lua_State *L ) {
  int stack = 0;

  // when we're called from an object the stack index 1 has our self ref 
  pulsecnt_t pc = pulsecnt_get(L, ++stack);

  // Get thresh0_val -- first arg after self arg
  int thresh0_val = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, thresh0_val >= -32768 && thresh0_val <= 32767, stack, "The thresh0_val number allows -32768 to 32767");

  // Get thresh0_val -- first arg after self arg
  int thresh1_val = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, thresh1_val >= -32768 && thresh1_val <= 32767, stack, "The thresh1_val number allows -32768 to 32767");

  // store it so we have it during callback
  pc->thresh0 = thresh0_val;
  pc->thresh1 = thresh1_val;

  /* Set threshold 0 and 1 values and enable events to watch */
  pcnt_set_event_value(pc->unit, PCNT_EVT_THRES_0, thresh0_val);
  pcnt_event_enable(pc->unit, PCNT_EVT_THRES_0);
  pcnt_set_event_value(pc->unit, PCNT_EVT_THRES_1, thresh1_val);
  pcnt_event_enable(pc->unit, PCNT_EVT_THRES_1);
  /* Enable events on zero, maximum and minimum limit values */
  pcnt_event_enable(pc->unit, PCNT_EVT_ZERO);
  pcnt_event_enable(pc->unit, PCNT_EVT_H_LIM);
  pcnt_event_enable(pc->unit, PCNT_EVT_L_LIM);

  /* Initialize PCNT's counter */
  pcnt_counter_pause(pc->unit);
  pcnt_counter_clear(pc->unit);

  // check if there's a callback otherwise don't trigger interrupt, instead they may just be polling
  if (pc->cb_ref != LUA_NOREF) {
    /* Register ISR handler and enable interrupts for PCNT unit */
    pcnt_isr_register(pulsecnt_intr_handler, NULL, 0, &user_isr_handle);
    pcnt_intr_enable(pc->unit);
  }

  /* Everything is set up, now go to counting */
  pcnt_counter_resume(pc->unit);
  
  if (pc->is_debug) ESP_LOGI("pulsecnt", "Setup threshold for unit %d with thr0 %d, thr1 %d", pc->unit, thresh0_val, thresh1_val);

  return 0;
}


// Lua: pc:rawSetEventVal(enumEventItem, val)
// enumEventItem can be pulsecnt.PCNT_EVT_L_LIM, PCNT_EVT_H_LIM, PCNT_EVT_THRES_0, PCNT_EVT_THRES_1, PCNT_EVT_ZERO
// Example: pc:rawSetEventVal(pulsecnt.PCNT_EVT_THRES_1, 100)
// The pulse counter is not cleared using this method so you can make the change on-the-fly, however, in practice
// it appears the pulse counter module does not pay attention to on-the-fly changes for the threshold value.
static int pulsecnt_set_event_value( lua_State *L ) {
  int stack = 0;

  // when we're called from an object the stack index 1 has our self ref 
  pulsecnt_t pc = pulsecnt_get(L, ++stack);


  // Get enum -- first arg after self arg
  int enumEventItem = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, enumEventItem >= -1 && enumEventItem <= 15, stack, "The enumEventItem number allows -1 to 15");

  // Get val -- 2nd arg after self arg
  int val = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, val >= -32768 && val <= 32767, stack, "The val number allows -32768 to 32767");

  /* Set value for this unit */
  pcnt_set_event_value(pc->unit, enumEventItem, val);

  // store it so we have it during callback and reset event or ESP32 won't accept in new val
  if (enumEventItem == PCNT_EVT_THRES_0) {
    pc->thresh0 = val;
    pcnt_event_enable(pc->unit, PCNT_EVT_THRES_0);
  } else if (enumEventItem == PCNT_EVT_THRES_1) {
    pc->thresh1 = val;
    pcnt_event_enable(pc->unit, PCNT_EVT_THRES_1);
  }
  
  // check if there's a callback otherwise don't trigger interrupt, instead they may just be polling
  if (pc->cb_ref != LUA_NOREF) {
    // even though we likely have the interrupt enabled, this re-reads the vals we just set?
    pcnt_intr_enable(pc->unit);
  }

  if (pc->is_debug) ESP_LOGI("pulsecnt", "Set enumEventItem %d for unit %d with val %d", enumEventItem, pc->unit, val);

  return 0;
}

// Lua: retVal = pc:rawGetEventVal(enumEventItem)
// enumEventItem can be pulsecnt.PCNT_EVT_L_LIM, PCNT_EVT_H_LIM, PCNT_EVT_THRES_0, PCNT_EVT_THRES_1, PCNT_EVT_ZERO
// Example: retVal = pc:rawGetEventVal(pulsecnt.PCNT_EVT_THRES_1)
static int pulsecnt_get_event_value( lua_State *L ) {
  int stack = 0;

  // when we're called from an object the stack index 1 has our self ref 
  pulsecnt_t pc = pulsecnt_get(L, ++stack);

  // Get enum -- first arg after self arg
  int enumEventItem = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, enumEventItem >= -1 && enumEventItem <= 15, stack, "The enumEventItem number allows -1 to 15");

  /* Set threshold 0 and 1 values and enable events to watch */
  // pcnt_get_event_value(pcnt_unit_tunit, pcnt_evt_type_tevt_type, int16_t *value)
  int16_t val;
  pcnt_get_event_value(pc->unit, enumEventItem, &val);
  
  if (pc->is_debug) ESP_LOGI("pulsecnt", "Get enumEventItem %d for unit %d with val %d", enumEventItem, pc->unit, val);

  lua_pushinteger (L, val);
  return 1;
}

// TODO: Not implemented yet.
// Lua example call:
// pc:config({
//   ch0: {
//     pulse_gpio_num = 4,
//     ctrl_gpio_num = 5,
//   },
//   ch1: {
//     pulse_gpio_num = 12,
//     ctrl_gpio_num = 14,
//   }
// })
static int pulsecnt_config(lua_State *L) {

  // We are passed in a complex Lua table
  int stack = 0;

  // when we're called from an object the stack index 1 has our self ref 
  pulsecnt_t pc = pulsecnt_get(L, ++stack);

  // get and set a self reference if we don't have one (which we likely won't have until this call occurs)
  if (pc->self_ref == LUA_NOREF) {
    lua_pushvalue(L, 1);
    pc->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  // check to make sure the next stack item is a Lua table
  int top = lua_gettop( L );

  for (int stack = 1; stack <= top; stack++) {
    if (lua_type( L, stack ) == LUA_TNIL)
      continue;

    if (lua_type( L, stack ) != LUA_TTABLE) {
      // ws2812_cleanup( L, 0 );
      luaL_checktype( L, stack, LUA_TTABLE ); // trigger error
      return 0;
    }

    //
    // retrieve ch0
    //
    lua_getfield( L, stack, "ch0" );
    if (lua_type( L, stack ) != LUA_TTABLE) {
      // ws2812_cleanup( L, 1 );
      return luaL_argerror( L, stack, "ch0 needs to be a table of settings" );
    }
    // int gpio_num = luaL_checkint( L, -1 );
    lua_pop( L, 1 );
  }

  return 0;
}

// This is called internally, not from Lua
static int pulsecnt_channel_config( lua_State *L, uint8_t channel ) {

  int stack = 0;

  // when we're called from an object the stack index 1 has our self ref 
  pulsecnt_t pc = pulsecnt_get(L, ++stack);

  // get and set a self reference if we don't have one (which we likely won't have until this call occurs)
  if (pc->self_ref == LUA_NOREF) {
    lua_pushvalue(L, 1);
    pc->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  // Get pulse_gpio_num -- first arg after self arg
  int pulse_gpio_num = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, pulse_gpio_num >= -1 && pulse_gpio_num <= 40, stack, "The pulse_gpio_num number allows -1 to 40");

  // Get ctrl_gpio_num -- 2nd arg
  int ctrl_gpio_num = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, ctrl_gpio_num >= -1 && ctrl_gpio_num <= 40, stack, "The ctrl_gpio_num number allows -1 to 40");

  // Get pos_mode -- 3rd arg
  int pos_mode = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, pos_mode >= 0 && pos_mode <= 2, stack, "The pos_mode number allows 0, 1, or 2");

  // Get neg_mode -- 4th arg
  int neg_mode = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, neg_mode >= 0 && neg_mode <= 2, stack, "The neg_mode number allows 0, 1, or 2");

  // Get lctrl_mode -- 5th arg
  int lctrl_mode = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, lctrl_mode >= 0 && lctrl_mode <= 2, stack, "The lctrl_mode number allows 0, 1, or 2");

  // Get hctrl_mode -- 6th arg
  int hctrl_mode = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, hctrl_mode >= 0 && hctrl_mode <= 2, stack, "The hctrl_mode number allows 0, 1, or 2");

  // Get counter_l_lim -- 7th arg. Defaults to -32767. Range int16 [-32768 : 32767]
  int counter_l_lim = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, counter_l_lim >= -32768 && counter_l_lim <= 32767, stack, "The counter_l_lim number allows -32768 to 32767");

  // Get counter_l_lim -- 7th arg. Defaults to -32767. Range int16 [-32768 : 32767]
  int counter_h_lim = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, counter_h_lim >= -32768 && counter_h_lim <= 32767, stack, "The counter_h_lim number allows -32768 to 32767");

  if (channel == 0) {
    pc->ch0_is_defined = true;
    pc->ch0_pulse_gpio_num = pulse_gpio_num;
    pc->ch0_ctrl_gpio_num = ctrl_gpio_num;
    pc->ch0_pos_mode = pos_mode;
    pc->ch0_neg_mode = neg_mode;
    pc->ch0_lctrl_mode = lctrl_mode;
    pc->ch0_hctrl_mode = hctrl_mode;
    pc->ch0_counter_l_lim = counter_l_lim;
    pc->ch0_counter_h_lim = counter_h_lim;
  } else {
    pc->ch1_is_defined = true;
    pc->ch1_pulse_gpio_num = pulse_gpio_num;
    pc->ch1_ctrl_gpio_num = ctrl_gpio_num;
    pc->ch1_pos_mode = pos_mode;
    pc->ch1_neg_mode = neg_mode;
    pc->ch1_lctrl_mode = lctrl_mode;
    pc->ch1_hctrl_mode = hctrl_mode;
    pc->ch1_counter_l_lim = counter_l_lim;
    pc->ch1_counter_h_lim = counter_h_lim;
  }

  /* Initialize PCNT functions:
  *  - configure and initialize PCNT
  *  - set up the input filter
  *  - set up the counter events to watch
  */

  /* Prepare configuration for the PCNT unit */
  pcnt_config_t pcnt_config = {
      // Set PCNT input signal and control GPIOs
      .pulse_gpio_num = pulse_gpio_num,
      .ctrl_gpio_num = ctrl_gpio_num,
      .channel = channel,
      .unit = pc->unit, // PCNT_TEST_UNIT
      // What to do on the positive / negative edge of pulse input?
      .pos_mode = pos_mode, //PCNT_COUNT_INC,   // Count up on the positive edge
      .neg_mode = neg_mode, //PCNT_COUNT_DIS,   // Keep the counter value on the negative edge
      // What to do when control input is low or high?
      .lctrl_mode = lctrl_mode, //PCNT_MODE_REVERSE, // Reverse counting direction if low
      .hctrl_mode = hctrl_mode, //PCNT_MODE_KEEP,    // Keep the primary counter mode if high
      // Set the maximum and minimum limit values to watch
      .counter_l_lim = counter_l_lim, //PCNT_L_LIM_VAL,
      .counter_h_lim = counter_h_lim, //PCNT_H_LIM_VAL,
  };
  /* Initialize PCNT unit */
  pcnt_unit_config(&pcnt_config);

  /* Enable events on zero, maximum and minimum limit values */
  if (pc->cb_ref != LUA_NOREF) { // if they didn't give callback, don't setup pcnt_isr_register
    pcnt_event_enable(pc->unit, PCNT_EVT_ZERO);
    pcnt_event_enable(pc->unit, PCNT_EVT_H_LIM);
    pcnt_event_enable(pc->unit, PCNT_EVT_L_LIM);
  }

  /* Initialize PCNT's counter */
  pcnt_counter_pause(pc->unit);
  pcnt_counter_clear(pc->unit);

  /* Register ISR handler and enable interrupts for PCNT unit */
  if (pc->cb_ref != LUA_NOREF) { // if they didn't give callback, don't setup pcnt_isr_register
    pcnt_isr_register(pulsecnt_intr_handler, NULL, 0, &user_isr_handle);
    pcnt_intr_enable(pc->unit);
  }
  /* Everything is set up, now go to counting */
  pcnt_counter_resume(pc->unit);

  if (pc->is_debug) ESP_LOGI("pulsecnt", "Channel %d config for unit %d, gpio: %d, ctrl_gpio: %d, chn: %d, pos_mode: %d, neg_mode: %d, lctrl_mode: %d, hctrl_mode: %d, counter_l_lim: %d, counter_h_lim: %d", channel, pc->unit, pulse_gpio_num, ctrl_gpio_num, PCNT_CHANNEL_0, pos_mode, neg_mode, lctrl_mode, hctrl_mode, counter_l_lim, counter_h_lim );
  
  return 0;
}

// Lua: pc:chan0Config(pulse_gpio_num, ctrl_gpio_num, pos_mode, neg_mode, lctrl_mode, hctrl_mode, counter_l_lim, counter_h_lim)
// Example: pc:chan0Config(2, 4, pulsecnt.PCNT_COUNT_INC, pulsecnt.PCNT_COUNT_DIS, pulsecnt.PCNT_MODE_REVERSE, pulsecnt.PCNT_MODE_KEEP, -100, 100)
static int pulsecnt_channel0_config( lua_State *L ) {
  return pulsecnt_channel_config(L, 0);
}
// Lua: pc:chan1Config(pulse_gpio_num, ctrl_gpio_num, pos_mode, neg_mode, lctrl_mode, hctrl_mode, counter_l_lim, counter_h_lim)
// Example: pc:chan1Config(2, 4, pulsecnt.PCNT_COUNT_INC, pulsecnt.PCNT_COUNT_DIS, pulsecnt.PCNT_MODE_REVERSE, pulsecnt.PCNT_MODE_KEEP, -100, 100)
static int pulsecnt_channel1_config( lua_State *L ) {
  return pulsecnt_channel_config(L, 1);
}

// Lua: pc = pulsecnt.create(unit, callback)
static int pulsecnt_create( lua_State *L ) {

  int stack = 0;

  // Get unit number -- first arg
  int unit = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, unit >= 0 && unit <= 7, stack, "The unit number allows 0 to 7");

  // Get callback method -- 2nd arg
  ++stack;
  // See if they even gave us a callback
  bool isCallback = true;
  if lua_isnoneornil(L, stack) {
    // user did not provide a callback. that's ok. just don't give them one.
    isCallback = false;
  } else {
    luaL_checkfunction(L, stack);
  }

  // ok, we have our unit number which is required. good. now create our object 
  pulsecnt_t pc = (pulsecnt_t)lua_newuserdata(L, sizeof(pulsecnt_struct_t));
  if (!pc) return luaL_error(L, "not enough memory");
  luaL_getmetatable(L, "pulsecnt.pctr");
  lua_setmetatable(L, -2);
  pc->cb_ref = LUA_NOREF;
  pc->self_ref = LUA_NOREF;
  pc->is_initted = false;
  pc->is_debug = false;
  pc->counter = 99;
  pc->unit = unit; // default to 0

  //get the lua function reference
  if (isCallback) {
    luaL_unref(L, LUA_REGISTRYINDEX, pc->cb_ref);
    lua_pushvalue(L, stack);
    pc->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  // store in our global static pulsecnt_selfs array for later reference during callback
  // where we only know the unit number 
  pulsecnt_selfs[unit] = pc;

  // Get is_debug -- 3rd arg optional
  ++stack;
  bool is_debug = luaL_optbool(L, stack, false);
  if (is_debug == 1) {
    pc->is_debug = true;
  }

  if (pc->is_debug) ESP_LOGI("pulsecnt", "Created obj for unit %d with callback ref of %ld", pc->unit, pc->cb_ref );

  return 1;
}

// Lua: pulsecnt:testCb( )
// This tests the callback where you can mimic in your code a sample callback as 
// part of your debugging process. Make sure to set your callback during the create() call.
static int pulsecnt_testCb(lua_State* L)
{
  // return 0;

  pulsecnt_t pc = pulsecnt_get(L, 1);

  // get a self reference if we don't have one
  if (pc->self_ref == LUA_NOREF) {
    lua_pushvalue(L, 1);
    pc->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  if (pc->cb_ref == LUA_NOREF) {
    luaL_error(L, "Callback not registered");
  }

  // post using lua task posting technique
  // on lua_open we set pulsecnt_task_id as a method which gets called
  // by Lua after task_post_high with reference to this self object and then we can steal the 
  // callback_ref and then it gets called by lua_call where we get to add our args
  // task_post_low(pulsecnt_task_id, (task_param_t)pc);
  task_post_low(pulsecnt_task_id, ( (0xffu) << 8) | (pc->unit) );
  

  return 0;
}

// Lua: pulsecnt:getCnt( )
// Get's the pulse counter for a unit.
static int pulsecnt_getCnt(lua_State* L)
{
  pulsecnt_t pc = pulsecnt_get(L, 1);

  int16_t count = 0;
  pcnt_get_counter_value(pc->unit, &count);
  if (pc->is_debug) ESP_LOGI("pulsecnt", "Got ctr val for unit %d with count of %d", pc->unit, count );
  pc->counter = count;

  lua_pushinteger(L, pc->counter);
  return 1;
}

// Lua: pulsecnt:clear( )
// Clear the pulse counter, i.e. set it back to 0.
static int pulsecnt_clear(lua_State* L)
{
  pulsecnt_t pc = pulsecnt_get(L, 1);
  pcnt_counter_clear(pc->unit);
  int16_t count = 0;
  pcnt_get_counter_value(pc->unit, &count);
  if (pc->is_debug) ESP_LOGI("pulsecnt", "Cleared ctr for unit %d with new count of %d", pc->unit, count );
  pc->counter = count;
  lua_pushinteger(L, pc->counter);
  return 1;
}

// Lua: pulsecnt:unregister( self )
static int pulsecnt_unregister(lua_State* L){
  pulsecnt_t pc = pulsecnt_get(L, 1);

  lua_pushinteger(L, pc->unit);
  if (pc->self_ref != LUA_REFNIL) {
    luaL_unref(L, LUA_REGISTRYINDEX, pc->self_ref);
    pc->self_ref = LUA_NOREF;
  }

  luaL_unref(L, LUA_REGISTRYINDEX, pc->cb_ref);
  pc->cb_ref = LUA_NOREF;

  return 0;
}

LROT_BEGIN(pulsecnt_dyn, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY( __gc,           pulsecnt_unregister )
  LROT_TABENTRY ( __index,        pulsecnt_dyn )
  // LROT_FUNCENTRY( __tostring,     pulsecnt_tostring )
  LROT_FUNCENTRY( getCnt,         pulsecnt_getCnt )
  LROT_FUNCENTRY( clear,          pulsecnt_clear )
  LROT_FUNCENTRY( testCb,         pulsecnt_testCb )
  LROT_FUNCENTRY( chan0Config,    pulsecnt_channel0_config )
  LROT_FUNCENTRY( chan1Config,    pulsecnt_channel1_config )
  LROT_FUNCENTRY( config,         pulsecnt_config )

  LROT_FUNCENTRY( setThres,       pulsecnt_set_thres )
  LROT_FUNCENTRY( setFilter,      pulsecnt_set_filter )
  LROT_FUNCENTRY( rawSetEventVal, pulsecnt_set_event_value )
  LROT_FUNCENTRY( rawGetEventVal, pulsecnt_get_event_value )
LROT_END(pulsecnt_dyn, NULL, LROT_MASK_GC_INDEX)

LROT_BEGIN(pulsecnt, NULL, 0)
  LROT_FUNCENTRY( create,            pulsecnt_create )
  LROT_NUMENTRY ( PCNT_MODE_KEEP,    0 ) /*pcnt_ctrl_mode_t.PCNT_MODE_KEEP*/
  LROT_NUMENTRY ( PCNT_MODE_REVERSE, 1 ) /*pcnt_ctrl_mode_t.PCNT_MODE_REVERSE*/
  LROT_NUMENTRY ( PCNT_MODE_DISABLE, 2 ) /*pcnt_ctrl_mode_t.PCNT_MODE_DISABLE*/
  LROT_NUMENTRY ( PCNT_COUNT_DIS,    0 ) /*pcnt_count_mode_t.PCNT_COUNT_DIS*/
  LROT_NUMENTRY ( PCNT_COUNT_INC,    1 ) /*pcnt_count_mode_t.PCNT_COUNT_INC*/
  LROT_NUMENTRY ( PCNT_COUNT_DEC,    2 ) /*pcnt_count_mode_t.PCNT_COUNT_DEC*/
  // LROT_NUMENTRY ( PCNT_COUNT_MAX ,   pcnt_count_mode_t.PCNT_COUNT_MAX )
  LROT_NUMENTRY ( PCNT_H_LIM_VAL,    32767 )
  LROT_NUMENTRY ( PCNT_L_LIM_VAL,    -32768 )
  LROT_NUMENTRY ( PCNT_EVT_L_LIM,    0 )
  LROT_NUMENTRY ( PCNT_EVT_H_LIM,    1 )
  LROT_NUMENTRY ( PCNT_EVT_THRES_0,  2 )
  LROT_NUMENTRY ( PCNT_EVT_THRES_1,  3 )
  LROT_NUMENTRY ( PCNT_EVT_ZERO,     4 )
  LROT_NUMENTRY ( PCNT_PIN_NOT_USED, -1 ) /*PCNT_PIN_NOT_USED*/
LROT_END(pulsecnt, NULL, 0)

int luaopen_pulsecnt(lua_State *L) {

  luaL_rometatable(L, "pulsecnt.pctr", LROT_TABLEREF(pulsecnt_dyn));

  pulsecnt_task_id = task_get_id(pulsecnt_task);

  return 0;
}

NODEMCU_MODULE(PULSECNT, "pulsecnt", pulsecnt, luaopen_pulsecnt);
