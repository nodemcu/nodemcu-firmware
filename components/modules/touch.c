/* 
Touch sensor module for ESP32 to allow interfacing from Lua
Authored by: ChiliPeppr (John Lauer) 2019

ESP-IDF docs for Touch Sensor
https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/touch_pad.html

ESP32 can handle up to 10 capacitive touch pads / GPIOs. 
The touch pad sensing process is under the control of a hardware-implemented finite-state 
machine (FSM) which is initiated by software or a dedicated hardware timer.

This example code is in the Public Domain (or CC0 licensed, at your option.)
Make modifications at will and freely.

Unless required by applicable law or agreed to in writing, this
software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.
*/

#include "module.h"
#include "common.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "task/task.h" 
#include "driver/touch_pad.h"
#include "esp_log.h"
#include "lextra.h"
#include "soc/rtc_periph.h"
#include "soc/touch_sensor_periph.h"

#include <string.h>

#define TOUCH_THRESH_NO_USE   (0)

static const char* TAG = "Touch";

typedef struct {
  // uint8_t selfs_index; // Keep track of our own index in touch_selfs so on cleanup we can unallocate
  int32_t cb_ref; // If a callback is provided, then we are using the ISR, otherwise we are just letting them poll
  bool is_initted;
  bool is_debug;
  bool touch_pads[TOUCH_PAD_MAX]; // TOUCH_PAD_MAX=10. Array of bools representing what touch pads are in this object
  uint16_t thres[TOUCH_PAD_MAX]; // Threshold values for each pad
  uint32_t filterMs;
  touch_high_volt_t hvolt;
  touch_low_volt_t lvolt;
  touch_volt_atten_t atten;
  uint8_t slope;
  bool is_intr;
  touch_trigger_mode_t thresTrigger;
} touch_struct_t;
typedef touch_struct_t *touch_t;

// array of 10 touch_struct_t pointers so we can reference by unit number
// this array gets filled in as we define touch_struct_t's during the create() method
// 10 pads are available, so leave enough room for 1 pad per create
static touch_t touch_self = NULL; // just allow 1 instance. used to allow 10, but changed approach. s[10] = {NULL};
// static uint8_t touch_selfs_last_index = 0; // keep track of last index used on touch_selfs

// Task ID to get ISR interrupt back into Lua callback
static task_handle_t touch_task_id;

// Helper function to create Lua tables in C
void l_pushtableintkeyval(lua_State* L , int key , int value) {
    lua_pushinteger(L, key);
    lua_pushinteger(L, value);
    lua_settable(L, -3);
} 
void l_pushtableintkeybool(lua_State* L , int key , bool value) {
    lua_pushinteger(L, key);
    lua_pushboolean(L, value);
    lua_settable(L, -3);
} 

/* This is the interrupt called by the hardware on a touch event.
 * Decode what touch pad originated the interrupt
 * and pass this information together with the event type
 * the main program so it can execute in the Lua process.
 */
static void IRAM_ATTR touch_intr_handler(void *arg)
{
  
  // Get the touch sensor status, usually used in ISR to decide which pads are ‘touched’.
  uint32_t pad_intr = touch_pad_get_status();
  //clear interrupt
  touch_pad_clear_status();
  

  // post using lua task posting technique
  // on lua_open we set touch_task_id as a method which gets called
  // by Lua after task_post_high with reference to this self object and then we can steal the 
  // callback_ref and then it gets called by lua_call where we get to add our args
  task_post_isr_high(touch_task_id, pad_intr );
  
}

/*
This method gets called from the IRAM interuppt method via Lua's task queue. That lets the interrupt 
run clean while this method gets called at a lower priority to not break the IRAM interrupt high priority.
We will do the actual callback here for the user with the fully decoded state of the touch pads that were triggered.
The format of the callback to your Lua code is:
  function onTouch(arrayOfPadsTouched)
*/
static void touch_task(task_param_t param, task_prio_t prio)
{
  (void)prio;

  //   if (touch_self != NULL && touch_self->is_debug) ESP_LOGI(TAG, "Got interrupt param: %d", param);

  // Now see which touch_selfs has cb_refs
  // For now just call all of them
  lua_State *L = lua_getstate();

  // old approach was to allow 10 objects. for now we are allowing 1, so go right to it.
  // for (int i = 0; i < touch_selfs_last_index; i++) {

  // see if there's any self and a callback on self, if so, do the callback
  if (touch_self != NULL) {
    if (touch_self->cb_ref != LUA_NOREF) {
      // we have a callback
      lua_rawgeti (L, LUA_REGISTRYINDEX, touch_self->cb_ref);
      const char* funcName = lua_tostring(L, -1);

      // create a table in c (it will be at the top of the stack)
      lua_newtable(L);

      // bool s_pad_activated[TOUCH_PAD_MAX];
      for (int i = 0; i < TOUCH_PAD_MAX; i++) {
        if ((param >> i) & 0x01) {
          // s_pad_activated[i] = true;
          l_pushtableintkeybool(L, i, true);
          //   if (touch_self->is_debug) ESP_LOGI(TAG, "Pushed key %d, bool %d", i, true);

        } else {
          // don't push false values for now to reduce memory usage
          //   l_pushtableintkeybool(L, i, false);
          //   if (touch_self->is_debug) ESP_LOGI(TAG, "Pushed key %d, bool %d", i, false);
        }
      }

      // call the cb_ref with one argument
      /* do the call (1 argument, 0 results) */
      if (luaL_pcallx(L, 1, 0) != 0) {
        ESP_LOGI(TAG, "error running callback function `f': %s", funcName);
      }
    }
  }

  //} // old for loop

}


/* 
Lua sample code:
tp = touch.create({
  pad = {8,9}, -- pad = 0 || {0,1,2,3,4,5,6,7,8,9} 0=GPIO4, 1=GPIO0, 2=GPIO2, 3=GPIO15, 4=GPIO13, 5=GPIO12, 6=GPIO14, 7=GPIO27, 8=GPIO33, 9=GPIO32
  cb = onTouch, -- Callback on touch event. Optional. If you do not provid you can just call touch:read(). If you do add a callback then TOUCH_FSM_MODE_TIMER is used.
  intrInitAtStart = false, -- Turn on interrupt at start. Default to true. Set to false in case you want to config first. Turn it on later with tp:intrEnable()
  thres = {[8]=300, [9]=500}, -- thres = 400 || {[8]=300, [9]=500} Pass in one uint16 (max 65536) or pass in table of pads with threshold value for each pad. Threshold of touchpad count. To get the right number, watch your touch pads for the high/low value. Run touch.test() to analyze all pads for their values.
  thresTrigger = TOUCH_TRIGGER_BELOW, -- Touch interrupt will happen if counter value is below or above threshold. TOUCH_TRIGGER_BELOW or TOUCH_TRIGGER_ABOVE 
  slope = 4, -- Touch sensor charge / discharge speed, 0-7 where 0 is no slope (so counter will always be 0), 1 is slow (lower counter), 7 is fastest (higher counter). Run the touch.test(pad) to test all combinations to pick best one.
  lvolt = TOUCH_LVOLT_0V5, -- Touch sensor low reference voltage TOUCH_LVOLT_0V4, TOUCH_LVOLT_0V5, TOUCH_LVOLT_0V6, TOUCH_LVOLT_0V7 
  hvolt = TOUCH_HVOLT_2V7, -- Touch sensor high reference voltage TOUCH_HVOLT_2V4, TOUCH_HVOLT_2V5, TOUCH_HVOLT_2V6, TOUCH_HVOLT_2V7
  atten = TOUCH_HVOLT_ATTEN_1V, -- Touch sensor high reference voltage attenuation TOUCH_HVOLT_ATTEN_0V, TOUCH_HVOLT_ATTEN_0V5, TOUCH_HVOLT_ATTEN_1V, TOUCH_HVOLT_ATTEN_1V5 
  isDebug = true
})
*/
static int touch_create( lua_State *L ) {

  // Check if we are out of objects. There are only 10 pads, so should never get this, but check anyhow.
  if (touch_self != NULL) {
    luaL_error(L, "Only 1 touch pad object allowed in touch library.");
  }

  // Presume we'll get a good create, so go ahead and make our touch_t object 
  touch_struct_t tpObj = {.cb_ref=LUA_NOREF, .is_initted=false, .is_debug=false};
  touch_t tp = &tpObj;

  // const int top = lua_gettop(L);
  luaL_checktable (L, 1);

  tp->is_debug = opt_checkbool(L, "isDebug", false);
  tp->filterMs = opt_checkint(L, "filterMs", 0);
  tp->lvolt = opt_checkint_range(L, "lvolt", TOUCH_LVOLT_KEEP, TOUCH_LVOLT_KEEP, TOUCH_LVOLT_MAX);
  tp->hvolt = opt_checkint_range(L, "hvolt", TOUCH_HVOLT_KEEP, TOUCH_HVOLT_KEEP, TOUCH_HVOLT_MAX);
  tp->atten = opt_checkint_range(L, "atten", TOUCH_HVOLT_ATTEN_KEEP, TOUCH_HVOLT_ATTEN_KEEP, TOUCH_HVOLT_ATTEN_MAX);
  tp->slope = opt_checkint_range(L, "slope", TOUCH_PAD_SLOPE_0, TOUCH_PAD_SLOPE_0, TOUCH_PAD_SLOPE_MAX);
  tp->is_intr = opt_checkbool(L, "intrInitAtStart", true);
  tp->thresTrigger = opt_checkint_range(L, "thresTrigger", TOUCH_TRIGGER_BELOW, TOUCH_TRIGGER_BELOW, TOUCH_TRIGGER_MAX);

  if (tp->is_debug) ESP_LOGI(TAG, "isDebug: %d, filterMs: %lu, lvolt: %d, hvolt: %d, atten: %d, slope: %d, intrInitAtStart: %d, thresTrigger: %d",
    tp->is_debug, tp->filterMs, tp->lvolt, tp->hvolt, tp->atten, tp->slope, tp->is_intr, tp->thresTrigger);

  // get the field pad. this can be passed in as int or table of ints. pad = 0 || {0,1,2,3,4,5,6,7,8,9}
  lua_getfield(L, 1, "pad");
  int type = lua_type(L, -1);
  if (type == LUA_TNUMBER) {
    int touch_pad_num = lua_tointeger(L, -1);
    luaL_argcheck(L, touch_pad_num >= 0 && touch_pad_num <= 9, -1, "The touch_pad_num allows 0 to 9");
    // opt_check(L, cond, name, extramsg)
    tp->touch_pads[touch_pad_num] = true;
    if (tp->is_debug) ESP_LOGI(TAG, "Set pad %d", touch_pad_num );
  }
  else if (type == LUA_TTABLE) {
  // else if (opt_get(L, "pad", LUA_TTABLE)) {
    lua_pushnil (L);
    while (lua_next(L, -2) != 0)
    {
      lua_pushvalue(L, -1); // copy, so lua_tonumber() doesn't break iter
      int touch_pad_num = lua_tointeger(L, -1);
      luaL_argcheck(L, touch_pad_num >= 0 && touch_pad_num <= 9, -1, "The touch_pad_num allows 0 to 9");
      tp->touch_pads[touch_pad_num] = true;
      lua_pop (L, 2); // leave key
      if (tp->is_debug) ESP_LOGI(TAG, "Set pad %d", touch_pad_num );
    }
  }
  else
    return luaL_error (L, "missing/bad 'pad' field");

  // See if they even gave us a callback
  bool isCallback = true;
  lua_getfield(L, 1, "cb");
  if lua_isnoneornil(L, -1) {
    // user did not provide a callback. that's ok. just don't give them one.
    isCallback = false;
    if (tp->is_debug) ESP_LOGI(TAG, "No callback provided. Not turning on interrupt." );
  } else {
    luaL_checkfunction(L, -1);

    //get the lua function reference
    luaL_unref(L, LUA_REGISTRYINDEX, tp->cb_ref);
    lua_pushvalue(L, -1);
    tp->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    if (tp->is_debug) ESP_LOGI(TAG, "Cb good." );
  }

  // Init
  /*The default FSM mode is ‘TOUCH_FSM_MODE_SW’. If you want to use interrupt trigger mode, 
  then set it using function ‘touch_pad_set_fsm_mode’ to ‘TOUCH_FSM_MODE_TIMER’ after 
  calling ‘touch_pad_init’. */
  esp_err_t err = touch_pad_init();
  if (err == ESP_FAIL) {
    ESP_LOGI(TAG, "Touch pad init error");
    return 0;
  } else {
    if (tp->is_debug) ESP_LOGI(TAG, "Initted touch pad");
  }

  // Set reference voltage for charging/discharging
  // For example, the high reference valtage will be 2.7V - 1V = 1.7V
  // The low reference voltage will be 0.5
  // The larger the range, the larger the pulse count value.
  touch_pad_set_voltage(tp->hvolt, tp->lvolt, tp->atten);
  if (tp->is_debug) ESP_LOGI(TAG, "Set voltage level hvolt: %d, lvolt: %d, atten: %d", tp->hvolt, tp->lvolt, tp->atten );

  // If use interrupt trigger mode, should set touch sensor FSM mode at 'TOUCH_FSM_MODE_TIMER'.
  if (isCallback == true) {

    if (tp->is_debug) ESP_LOGI(TAG, "Setting FSM mode since you provided a callback");
    err = touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    if (err == ESP_FAIL) {
      ESP_LOGI(TAG, "Touch pad set fsm mode error");
      return 0;
    }

    for (int padnum = 0; padnum< TOUCH_PAD_MAX; padnum++) {
      // see if we have a pad by this number to config
      if (tp->touch_pads[padnum] == true) {
        //init RTC IO and mode for touch pad.
        touch_pad_config(padnum, tp->thres[padnum]);
        if (tp->is_debug) ESP_LOGI(TAG, "Did config for pad %d with thres %d", padnum, tp->thres[padnum]);
      }
    }

    // Initialize and start a software filter to detect slight change of capacitance.
    if (tp->filterMs > 0) {
      touch_pad_filter_start(tp->filterMs);
      if (tp->is_debug) ESP_LOGI(TAG, "Set filter period to %lu ms", tp->filterMs );
    }

    // Register touch interrupt ISR
    touch_pad_isr_register(touch_intr_handler, NULL);
    if (tp->is_debug) ESP_LOGI(TAG, "Registered ISR handler for callback" );

    // set trigger mode
    touch_pad_set_trigger_mode(tp->thresTrigger);

    if (tp->is_intr) {
      touch_pad_intr_enable();
      if (tp->is_debug) ESP_LOGI(TAG, "Enabled interrupt" );
    }
  
  } else {

    // No callback mode. Just polling for values.
    if (tp->is_debug) ESP_LOGI(TAG, "No callback provided, so no interrupt or threshold set." );
    touch_pad_intr_disable();
    err = touch_pad_set_fsm_mode(TOUCH_FSM_MODE_SW);
    if (err == ESP_FAIL) {
      ESP_LOGI(TAG, "Touch pad set fsm mode to sw error");
      return 0;
    } else {
      if (tp->is_debug) ESP_LOGI(TAG, "Touch pad set fsm mode to sw since no callback");
    }

    // set THRES_NO_USE since in polling mode
    for (int padnum = 0; padnum< TOUCH_PAD_MAX; padnum++) {
      // see if we have a pad by this number to config
      if (tp->touch_pads[padnum] == true) {
        //init RTC IO and mode for touch pad.
        touch_pad_config(padnum, TOUCH_THRESH_NO_USE);
        if (tp->is_debug) ESP_LOGI(TAG, "Did config for pad %d with thres %d", padnum, TOUCH_THRESH_NO_USE);
      }
    }

    // see if they want a filter, if so turn it on
    // start touch pad filter function This API will start a filter to process the noise in order to 
    // prevent false triggering when detecting slight change of capacitance. Need to call 
    // touch_pad_filter_start before all touch filter APIs
    if (tp->filterMs > 0) {
      if (tp->is_debug) ESP_LOGI(TAG, "You provided a filter so turning on filter mode. filterMs: %lu", tp->filterMs);
      esp_err_t err = touch_pad_filter_start(tp->filterMs);
      if (err == ESP_ERR_INVALID_ARG) {
        ESP_LOGI(TAG, "Filter start parameter error");
      } else if (err == ESP_ERR_NO_MEM) {
        ESP_LOGI(TAG, "Filter no memory for driver");
      } else if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "Filter driver state error");
      }
    }
  }

  // if (tp->is_debug) ESP_LOGI(TAG, "Created obj with callback ref of %d", tp->cb_ref );

  // Now create our Lua version of this data to pass back
  touch_t tp2 = (touch_t)lua_newuserdata(L, sizeof(touch_struct_t));
  if (!tp2) return luaL_error(L, "not enough memory");
  luaL_getmetatable(L, "touch.pctr");
  lua_setmetatable(L, -2);
  tp2->cb_ref = tp->cb_ref;
  // tp2->self_ref = tp->self_ref;
  tp2->is_initted = tp->is_initted;
  tp2->is_debug = tp->is_debug;
  tp2->filterMs = tp->filterMs;
  tp2->hvolt = tp->hvolt;
  tp2->lvolt = tp->lvolt;
  tp2->atten = tp->atten;
  tp2->slope = tp->slope;
  
  for (int i = 0; i < 10; i++) {
    tp2->touch_pads[i] = tp->touch_pads[i];
  }

  // We need to store this in self_refs so we have a way to look for the cb_ref from the IRAM interrupt
  touch_self = tp2; 

  return 1;
}

// Get the touch.pctr object from the stack which is the struct touch_t
static touch_t touch_get( lua_State *L, int stack )
{
  return (touch_t)luaL_checkudata(L, stack, "touch.pctr");
}

// Lua: touch:setTriggerMode(mode) -- TOUCH_TRIGGER_BELOW or TOUCH_TRIGGER_ABOVE
static int touch_setTriggerMode(lua_State* L)
{
  touch_t tp = touch_get(L, 1);

  int stack = 1;

  // Get mode -- first arg after self arg
  int thresTrigger = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, thresTrigger >= 0 && thresTrigger <= 1, -1, "The thresTrigger allows 0 or 1");

  tp->thresTrigger = thresTrigger;
  touch_pad_set_trigger_mode(tp->thresTrigger);

  if (tp->is_debug) ESP_LOGI(TAG, "Set thresTrigger to %d", tp->thresTrigger);

  return 0;
}

// Lua: touch:setThres(padNum, thresVal)
// Set touch sensor interrupt threshold.
static int touch_setThres(lua_State* L)
{
  touch_t tp = touch_get(L, 1);

  int stack = 1;

  // Get padNum -- first arg after self arg
  int padNum = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, padNum >= 0 && padNum <= 10, stack, "The padNum number allows 0 to 10");

  // Get thresVal -- first arg after self arg
  int thresVal = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, thresVal >= 0 && thresVal <= 65536, stack, "The thresVal number allows 0 to 65536");

  touch_pad_set_thresh(padNum, thresVal);

  tp->thres[padNum] = thresVal;

  if (tp->is_debug) ESP_LOGI(TAG, "Set threshold for pad %d val %d", padNum, thresVal);

  return 0;
}

// Lua: touch:intrEnable()
static int touch_intrEnable(lua_State* L)
{
  touch_t tp = touch_get(L, 1);

  tp->is_intr = true;

  touch_pad_intr_enable();

  if (tp->is_debug) ESP_LOGI(TAG, "Turned on touch pad interrupt");

  return 0;
}

// Lua: touch:intrDisable()
static int touch_intrDisable(lua_State* L)
{
  touch_t tp = touch_get(L, 1);

  tp->is_intr = false;

  touch_pad_intr_disable();
  touch_pad_clear_status();

  if (tp->is_debug) ESP_LOGI(TAG, "Turned off touch pad interrupt");

  return 0;
}

// Lua: tbl = touch:read()
// Get touch sensor counter value. Each touch sensor has a counter to count the 
// number of charge/discharge cycles. When the pad is not ‘touched’, we can get 
// a number of the counter. When the pad is ‘touched’, the value in counter will 
// get smaller because of the larger equivalent capacitance.
static int touch_read(lua_State* L)
{
  touch_t tp = touch_get(L, 1);

  // return 1 parameter by default, unless in filter mode, then return 2 params
  uint8_t numRetVals = 1;

  // see if we are in interrupt mode or polling mode
  // see if we are in filter mode or non-filter
  // if in filter mode, we need to do raw_read and filter_read
  // in non-filter mode, we just need to do read

  if (tp->cb_ref == LUA_NOREF && tp->filterMs > 0) {
    // we are in polling mode and we are in filter mode
    // so do 2 params for raw and filter

    numRetVals = 2;

    // we will send back 2 params of format:
    // pads will be {"0":rawval, "1":rawval, ...}
    // filtervals will be {"0":filtval, "1":filtval, ...}

    // do raw vals
    // create a table in c (it will be at the top of the stack)
    lua_newtable(L);

    uint16_t touch_value = 0;
    for (int i = 0; i < TOUCH_PAD_MAX; i++) {

      // see if this object wants a touch read for this pad
      if (tp->touch_pads[i] == true) {
        // they do want a touch read for this pad
        esp_err_t err = touch_pad_read_raw_data((touch_pad_t)i, &touch_value);

        if (err == ESP_ERR_INVALID_ARG) {
          luaL_error(L, "Touch pad parameter error");
        } else if (err == ESP_ERR_INVALID_STATE) {
          luaL_error(L, "Touch pad hardware connection has an error, the value of touch_value is 0.");
        } else if (err == ESP_FAIL) {
          luaL_error(L, "Touch pad not initialized");
        } else {

          l_pushtableintkeyval(L, i, touch_value);

          if (tp->is_debug) ESP_LOGI(TAG, "Got touch raw val for pin %d of %d", i, touch_value );

        }

      }
    }

    // do filter vals
    // create a table in c (it will be at the top of the stack)
    lua_newtable(L);

    // uint16_t touch_value = 0;
    for (int i = 0; i < TOUCH_PAD_MAX; i++) {

      // see if this object wants a touch read for this pad
      if (tp->touch_pads[i] == true) {
        // they do want a touch read for this pad
        esp_err_t err = touch_pad_read_filtered((touch_pad_t)i, &touch_value);

        if (err == ESP_ERR_INVALID_ARG) {
          luaL_error(L, "Touch pad parameter error");
        } else if (err == ESP_ERR_INVALID_STATE) {
          luaL_error(L, "Touch pad hardware connection has an error, the value of touch_value is 0.");
        } else if (err == ESP_FAIL) {
          luaL_error(L, "Touch pad not initialized");
        } else {

          l_pushtableintkeyval(L, i, touch_value);

          if (tp->is_debug) ESP_LOGI(TAG, "Got touch filtered val for pin %d of %d", i, touch_value );

        }

      }
    }

  } else {
    // we are in non-filter mode. do normal read and send 1 param back.

    // esp_err_t touch_pad_read(touch_pad_t touch_num, uint16_t *touch_value)

    // we will send back data in the format: {"0":val, "1":val}
    // create a table in c (it will be at the top of the stack)
    lua_newtable(L);

    uint16_t touch_value = 0;
    for (int i = 0; i < TOUCH_PAD_MAX; i++) {

      // see if this object wants a touch read for this pad
      if (tp->touch_pads[i] == true) {
        // they do want a touch read for this pad
        esp_err_t err = touch_pad_read((touch_pad_t)i, &touch_value);

        if (err == ESP_ERR_INVALID_ARG) {
          luaL_error(L, "Touch pad parameter error");
        } else if (err == ESP_ERR_INVALID_STATE) {
          luaL_error(L, "Touch pad hardware connection has an error, the value of touch_value is 0.");
        } else if (err == ESP_FAIL) {
          luaL_error(L, "Touch pad not initialized");
        } else {

          l_pushtableintkeyval(L, i, touch_value);

          if (tp->is_debug) ESP_LOGI(TAG, "Got touch val for pin %d of %d", i, touch_value );

        }

      }
    }
  }

  // the table (or tables) is in the stack, it should get passed back
  return numRetVals;
}


// Lua: touch:unregister( self )
static int touch_unregister(lua_State* L) {
  touch_t tp = touch_get(L, 1);

  // if there was a callback, turn off ISR
  if (tp->cb_ref != LUA_NOREF) {
    touch_intrDisable(L);
    
    touch_pad_isr_deregister(touch_intr_handler, NULL);

    luaL_unref(L, LUA_REGISTRYINDEX, tp->cb_ref);
    tp->cb_ref = LUA_NOREF;
    
  } else {
    // non-interrupt mode
    if (tp->filterMs > 0) {
      touch_pad_filter_stop();
    }
  }

  touch_pad_deinit();
  touch_self = NULL;

  return 0;
}

LROT_BEGIN(touch_dyn, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY( __gc,           touch_unregister )
  LROT_TABENTRY ( __index,        touch_dyn )
  // LROT_FUNCENTRY( __tostring,     touch_tostring )
  LROT_FUNCENTRY( read,           touch_read )
  LROT_FUNCENTRY( intrEnable,     touch_intrEnable )
  LROT_FUNCENTRY( intrDisable,    touch_intrDisable )
  LROT_FUNCENTRY( setThres,       touch_setThres )
  LROT_FUNCENTRY( setTriggerMode, touch_setTriggerMode )
LROT_END(touch_dyn, NULL, LROT_MASK_GC_INDEX)

LROT_BEGIN(touch, NULL, 0)
  LROT_FUNCENTRY( create,            touch_create )
  LROT_NUMENTRY ( TOUCH_HVOLT_KEEP,    TOUCH_HVOLT_KEEP )
  LROT_NUMENTRY ( TOUCH_HVOLT_2V4,    TOUCH_HVOLT_2V4 )
  LROT_NUMENTRY ( TOUCH_HVOLT_2V5,    TOUCH_HVOLT_2V5 )
  LROT_NUMENTRY ( TOUCH_HVOLT_2V6,    TOUCH_HVOLT_2V6 )
  LROT_NUMENTRY ( TOUCH_HVOLT_2V7,    TOUCH_HVOLT_2V7 )
  LROT_NUMENTRY ( TOUCH_HVOLT_MAX,    TOUCH_HVOLT_MAX )
  LROT_NUMENTRY ( TOUCH_LVOLT_KEEP,    TOUCH_LVOLT_KEEP )
  LROT_NUMENTRY ( TOUCH_LVOLT_0V5,    TOUCH_LVOLT_0V5 )
  LROT_NUMENTRY ( TOUCH_LVOLT_0V6,    TOUCH_LVOLT_0V6 )
  LROT_NUMENTRY ( TOUCH_LVOLT_0V7,    TOUCH_LVOLT_0V7 )
  LROT_NUMENTRY ( TOUCH_LVOLT_0V8,    TOUCH_LVOLT_0V8 )
  LROT_NUMENTRY ( TOUCH_LVOLT_MAX,    TOUCH_LVOLT_MAX )
  LROT_NUMENTRY ( TOUCH_HVOLT_ATTEN_KEEP,    TOUCH_HVOLT_ATTEN_KEEP )
  LROT_NUMENTRY ( TOUCH_HVOLT_ATTEN_1V5,    TOUCH_HVOLT_ATTEN_1V5 )
  LROT_NUMENTRY ( TOUCH_HVOLT_ATTEN_1V,    TOUCH_HVOLT_ATTEN_1V )
  LROT_NUMENTRY ( TOUCH_HVOLT_ATTEN_0V5,    TOUCH_HVOLT_ATTEN_0V5 )
  LROT_NUMENTRY ( TOUCH_HVOLT_ATTEN_0V,    TOUCH_HVOLT_ATTEN_0V )
  LROT_NUMENTRY ( TOUCH_TRIGGER_BELOW,    TOUCH_TRIGGER_BELOW )
  LROT_NUMENTRY ( TOUCH_TRIGGER_ABOVE,    TOUCH_TRIGGER_ABOVE )
LROT_END(touch, NULL, 0)

int luaopen_touch(lua_State *L) {

  luaL_rometatable(L, "touch.pctr", LROT_TABLEREF(touch_dyn));

  touch_task_id = task_get_id(touch_task);

  return 0;
}

NODEMCU_MODULE(TOUCH, "touch", touch, luaopen_touch);
