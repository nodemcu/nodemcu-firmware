#include <pm/pmSleep.h>
#ifdef  PMSLEEP_ENABLE
#define STRINGIFY_VAL(x) #x
#define STRINGIFY(x) STRINGIFY_VAL(x)

//TODO: figure out why timed light_sleep doesn't work

//holds duration error string
//uint32 PMSLEEP_SLEEP_MAX_TIME=FPM_SLEEP_MAX_TIME-1;
const char *PMSLEEP_DURATION_ERR_STR="duration: 0 or "STRINGIFY(PMSLEEP_SLEEP_MIN_TIME)"-"STRINGIFY(PMSLEEP_SLEEP_MAX_TIME)" us";


/*  INTERNAL VARIABLES  */
static void (*user_suspend_cb)(void) = NULL;
static void (*user_resume_cb)(void) = NULL;
static uint8 resume_opmode = 0;
static os_timer_t wifi_suspended_test_timer;
static uint8 autosleep_setting_temp = 0;
static os_timer_t null_mode_check_timer;
static pmSleep_param_t current_config;


/*  INTERNAL FUNCTION DECLARATIONS  */
static void suspend_all_timers(void);
static void null_mode_check_timer_cb(void* arg);
static void resume_all_timers(void);
static inline void register_lua_cb(lua_State* L,int* cb_ref);
static void resume_cb(void);
static void wifi_suspended_timer_cb(int arg);

/*  INTERNAL FUNCTIONS  */

static void suspend_all_timers(void){
#ifdef TIMER_SUSPEND_ENABLE
  extern void swtmr_suspend_timers();
  swtmr_suspend_timers();
#endif
  return;
}

static void resume_all_timers(void){
#ifdef TIMER_SUSPEND_ENABLE
  extern void swtmr_resume_timers();
  swtmr_resume_timers();
#endif
  return;
}

static void null_mode_check_timer_cb(void* arg){
  if (wifi_get_opmode() == NULL_MODE){
    //check if uart 0 tx buffer is empty and uart 1 tx buffer is empty
    if(current_config.sleep_mode == LIGHT_SLEEP_T){
      if((READ_PERI_REG(UART_STATUS(0)) & (UART_TXFIFO_CNT<<UART_TXFIFO_CNT_S)) == 0 &&
         (READ_PERI_REG(UART_STATUS(1)) & (UART_TXFIFO_CNT<<UART_TXFIFO_CNT_S)) == 0){
        os_timer_disarm(&null_mode_check_timer);
        suspend_all_timers();
        //Ensure UART 0/1 TX FIFO is clear
        SET_PERI_REG_MASK(UART_CONF0(0), UART_TXFIFO_RST);//RESET FIFO
        CLEAR_PERI_REG_MASK(UART_CONF0(0), UART_TXFIFO_RST);
        SET_PERI_REG_MASK(UART_CONF0(1), UART_TXFIFO_RST);//RESET FIFO
        CLEAR_PERI_REG_MASK(UART_CONF0(1), UART_TXFIFO_RST);
        wifi_fpm_do_sleep(current_config.sleep_duration);
        return;
      }
      else{
        return;
      }
    }
    else{  //MODEM_SLEEP_T
      sint8 retval_wifi_fpm_do_sleep = wifi_fpm_do_sleep(current_config.sleep_duration); // Request WiFi suspension and store return value

      // If wifi_fpm_do_sleep success
      if (retval_wifi_fpm_do_sleep == 0){
        PMSLEEP_DBG("wifi_fpm_do_sleep success, starting wifi_suspend_test timer");
        os_timer_disarm(&wifi_suspended_test_timer);
        os_timer_setfn(&wifi_suspended_test_timer, (os_timer_func_t*)wifi_suspended_timer_cb, NULL);
          //The callback wifi_suspended_timer_cb detects when the esp8266 has successfully entered modem_sleep and executes the developer's suspend_cb.
          //Since this timer is only used in modem_sleep and will never be active outside of modem_sleep, it is unnecessary to register the cb with SWTIMER_REG_CB.
        os_timer_arm(&wifi_suspended_test_timer, 1, 1);
      }
      else{ // This should never happen. if it does, return the value for error reporting
        wifi_fpm_close();
        PMSLEEP_ERR("wifi_fpm_do_sleep returned %d", retval_wifi_fpm_do_sleep);
      }
    }
    os_timer_disarm(&null_mode_check_timer);
    return;
  }
}


//function to register a lua callback function in the LUA_REGISTRYINDEX for later execution
static inline void register_lua_cb(lua_State* L, int* cb_ref){
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  if( *cb_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, *cb_ref);
  *cb_ref = ref;
}

// C callback for bringing WiFi back from forced sleep
static void resume_cb(void){
  PMSLEEP_DBG("START");
  //TODO: Add support for extended light sleep duration
  wifi_fpm_close(); // Disable force sleep API
  PMSLEEP_DBG("WiFi resume");

  resume_all_timers();

  //this section restores null mode auto sleep setting
  if(autosleep_setting_temp == 1)  {
    wifi_fpm_auto_sleep_set_in_null_mode(autosleep_setting_temp);
    autosleep_setting_temp = 0;
  }

  //this section restores previous wifi mode
  if (resume_opmode == STATION_MODE || resume_opmode == SOFTAP_MODE || resume_opmode == STATIONAP_MODE){
    if (wifi_set_opmode_current(resume_opmode)){
      if (resume_opmode == STATION_MODE || resume_opmode == STATIONAP_MODE){
        wifi_station_connect(); // Connect to currently configured Access Point
      }
      PMSLEEP_DBG("WiFi mode restored");
      resume_opmode = 0; // reset variable to default value
    }
  }
  else{
    wifi_set_opmode_current(NULL_MODE);
  }

  //execute the external resume callback
  if (user_resume_cb != NULL){
    PMSLEEP_DBG("calling user resume cb (%p)", user_resume_cb);
    user_resume_cb();
    user_resume_cb = NULL;
  }

  PMSLEEP_DBG("END");
  return;
}

// This callback executes the suspended callback when Wifi suspension is in effect
static void wifi_suspended_timer_cb(int arg){
  // check if wifi is suspended.
  if (pmSleep_get_state() == PMSLEEP_SUSPENDED){
    os_timer_disarm(&wifi_suspended_test_timer); // Stop rf_closed_timer
    PMSLEEP_DBG("WiFi is suspended");

    //execute the external suspended callback
    if (user_suspend_cb != NULL){
      PMSLEEP_DBG("calling user suspend cb (%p)", user_suspend_cb);
      user_suspend_cb();
      user_suspend_cb = NULL;
    }
    PMSLEEP_DBG("END");
  }
}

/*  EXTERNAL FUNCTIONS  */

//this function executes the application developer's Lua callback
void pmSleep_execute_lua_cb(int* cb_ref){
  if (*cb_ref != LUA_NOREF){
    lua_State* L = lua_getstate(); // Get Lua main thread pointer
    lua_rawgeti(L, LUA_REGISTRYINDEX, *cb_ref); // Push resume callback onto the stack
    lua_unref(L, *cb_ref); // Remove resume callback from registry
    *cb_ref = LUA_NOREF; // Update variable since reference is no longer valid
    lua_call(L, 0, 0); // Execute resume callback
  }

}

//this function checks current wifi suspension state and returns the result
uint8 pmSleep_get_state(void){
  if (fpm_rf_is_closed()) return PMSLEEP_SUSPENDED;
  else if (fpm_is_open()) return PMSLEEP_SUSPENSION_PENDING;
  else return PMSLEEP_AWAKE;
}

//this function parses the lua configuration table provided by the application developer
int pmSleep_parse_table_lua( lua_State* L, int table_idx, pmSleep_param_t *cfg, int *suspend_lua_cb_ref, int *resume_lua_cb_ref){
  lua_Integer Linteger_tmp = 0;

  if( cfg->sleep_mode == MODEM_SLEEP_T ){ //WiFi suspend
    lua_getfield(L, table_idx, "duration");
    if( !lua_isnil(L, -1) ){  /* found? */
      if( lua_isnumber(L, -1) ){
        lua_Integer Linteger=luaL_checkinteger(L, -1);
        luaL_argcheck(L,(((Linteger >= PMSLEEP_SLEEP_MIN_TIME) && (Linteger <= PMSLEEP_SLEEP_MAX_TIME)) ||
            (Linteger == 0)), table_idx, PMSLEEP_DURATION_ERR_STR);
        cfg->sleep_duration = (uint32)Linteger; // Get suspend duration
      }
      else{
        return luaL_argerror( L, table_idx, "duration: must be number" );
      }
    }
    else{
      return luaL_argerror( L, table_idx, PMSLEEP_DURATION_ERR_STR );
    }
    lua_pop(L, 1);

    lua_getfield(L, table_idx, "suspend_cb");
    if( !lua_isnil(L, -1) ){  /* found? */
      if( lua_isfunction(L, -1) ){
        lua_pushvalue(L, -1); // Push resume callback to the top of the stack
        register_lua_cb(L, suspend_lua_cb_ref);
      }
      else{
        return luaL_argerror( L, table_idx, "suspend_cb: must be function" );
      }
    }
    lua_pop(L, 1);
  }
  else if (cfg->sleep_mode == LIGHT_SLEEP_T){ //CPU suspend
#ifdef TIMER_SUSPEND_ENABLE
    lua_getfield(L, table_idx, "wake_pin");
    if( !lua_isnil(L, -1) ){  /* found? */
      if( lua_isnumber(L, -1) ){
        Linteger_tmp=lua_tointeger(L, -1);
        luaL_argcheck(L, (platform_gpio_exists(Linteger_tmp) && Linteger_tmp > 0), table_idx, "wake_pin: Invalid interrupt pin");
        cfg->wake_pin = Linteger_tmp;
      }
      else{
        return luaL_argerror( L, table_idx, "wake_pin: must be number" );
      }
    }
    else{
      return luaL_argerror( L, table_idx, "wake_pin: must specify pin" );
//    else if(cfg->sleep_duration == 0){
//      return luaL_argerror( L, table_idx, "wake_pin: must specify pin if sleep duration is indefinite" );
  }
    lua_pop(L, 1);

    lua_getfield(L, table_idx, "int_type");
    if( !lua_isnil(L, -1) ){  /* found? */
      if( lua_isnumber(L, -1) ){
        Linteger_tmp=lua_tointeger(L, -1);
        luaL_argcheck(L, (Linteger_tmp == GPIO_PIN_INTR_ANYEDGE || Linteger_tmp == GPIO_PIN_INTR_HILEVEL
            || Linteger_tmp == GPIO_PIN_INTR_LOLEVEL || Linteger_tmp == GPIO_PIN_INTR_NEGEDGE
            || Linteger_tmp == GPIO_PIN_INTR_POSEDGE ), 1, "int_type: invalid interrupt type");
        cfg->int_type = Linteger_tmp;
      }
      else{
        return luaL_argerror( L, table_idx, "int_type: must be number" );
      }
    }
    else{
      cfg->int_type = GPIO_PIN_INTR_LOLEVEL;
    }
    lua_pop(L, 1);
#endif
  }
  else{
    return luaL_error(L, "FPM Sleep mode not available");
  }

  lua_getfield(L, table_idx, "resume_cb");
  if( !lua_isnil(L, -1) ){  /* found? */
    if( lua_isfunction(L, -1) ){
      lua_pushvalue(L, -1); // Push resume callback to the top of the stack
      register_lua_cb(L, resume_lua_cb_ref);
    }
    else{
      return luaL_argerror( L, table_idx, "resume_cb: must be function" );
    }
  }
  lua_pop(L, 1);

  lua_getfield(L, table_idx, "preserve_mode");
  if( !lua_isnil(L, -1) ){  /* found? */
    if( lua_isboolean(L, -1) ){
      cfg->preserve_opmode=lua_toboolean(L, -1);
    }
    else{
      return luaL_argerror( L, table_idx, "preseve_mode: must be boolean" );
    }
  }
  else{
    cfg->preserve_opmode = true;
  }
  lua_pop(L, 1);

  //if sleep duration is zero, set indefinite sleep duration
  if( cfg->sleep_duration == 0 ){
    cfg->sleep_duration = FPM_SLEEP_MAX_TIME;
  }

  return 0;
}

//This function resumes ESP from MODEM_SLEEP
void pmSleep_resume(void (*resume_cb_ptr)(void)){
  PMSLEEP_DBG("START");
  uint8 fpm_state = pmSleep_get_state();

  if(fpm_state>0){
    if(resume_cb_ptr != NULL){
      user_resume_cb = resume_cb_ptr;
    }
    wifi_fpm_do_wakeup(); // Wake up from sleep
    resume_cb(); // Finish WiFi wakeup
  }

  PMSLEEP_DBG("END");
  return;
}

//this function puts the ESP8266 into MODEM_SLEEP or LIGHT_SLEEP
void pmSleep_suspend(pmSleep_param_t *cfg){
  PMSLEEP_DBG("START");

  lua_State* L = lua_getstate();
#ifndef TIMER_SUSPEND_ENABLE
  if(cfg->sleep_mode == LIGHT_SLEEP_T){
    luaL_error(L, "timer suspend API is disabled, light sleep unavailable");
    return;
  }
#endif
  uint8 current_wifi_mode = wifi_get_opmode(); // Get Current WiFi mode

  user_resume_cb = cfg->resume_cb_ptr; //pointer to hold address of user_cb
  user_suspend_cb = cfg->suspend_cb_ptr; //pointer to hold address of user_cb

  // If Preserve_wifi_mode parameter is TRUE and current WiFi mode is not NULL_MODE
  if (cfg->preserve_opmode && current_wifi_mode != 0){
    resume_opmode = current_wifi_mode;
  }


  if (current_wifi_mode == STATION_MODE || current_wifi_mode == STATIONAP_MODE){
    wifi_station_disconnect(); // Disconnect from Access Point
  }

  //the null mode sleep functionality interferes with the forced sleep API and must be disabled
  if(get_fpm_auto_sleep_flag() == 1){
    autosleep_setting_temp = 1;
    wifi_fpm_auto_sleep_set_in_null_mode(0);
  }

  // If wifi_set_opmode_current is successful
  if (wifi_set_opmode_current(NULL_MODE)){
    PMSLEEP_DBG("sleep_mode is %s", cfg->sleep_mode == MODEM_SLEEP_T ? "MODEM_SLEEP_T" : "LIGHT_SLEEP_T");

    wifi_fpm_set_sleep_type(cfg->sleep_mode);

    wifi_fpm_open(); // Enable force sleep API

    if (cfg->sleep_mode == LIGHT_SLEEP_T){
#ifdef TIMER_SUSPEND_ENABLE
      if(platform_gpio_exists(cfg->wake_pin) && cfg->wake_pin > 0){
        PMSLEEP_DBG("Wake-up pin is %d\t interrupt type is %d", cfg->wake_pin, cfg->int_type);

        if((cfg->int_type != GPIO_PIN_INTR_ANYEDGE && cfg->int_type != GPIO_PIN_INTR_HILEVEL
            && cfg->int_type != GPIO_PIN_INTR_LOLEVEL && cfg->int_type != GPIO_PIN_INTR_NEGEDGE
            && cfg->int_type != GPIO_PIN_INTR_POSEDGE )){
          wifi_fpm_close();
          PMSLEEP_DBG("Invalid interrupt type");
          return;
        }

        GPIO_DIS_OUTPUT(pin_num[cfg->wake_pin]);
        PIN_FUNC_SELECT(pin_mux[cfg->wake_pin], pin_func[cfg->wake_pin]);
        wifi_enable_gpio_wakeup(pin_num[cfg->wake_pin], cfg->int_type);
      }
      else if(cfg->sleep_duration == FPM_SLEEP_MAX_TIME && cfg->wake_pin == 255){
        wifi_fpm_close();
        PMSLEEP_DBG("No wake-up pin defined");
        return;
      }
#endif

    }

    wifi_fpm_set_wakeup_cb(resume_cb); // Set resume C callback

    c_memcpy(&current_config, cfg, sizeof(pmSleep_param_t));
    PMSLEEP_DBG("sleep duration is %d", current_config.sleep_duration);

    os_timer_disarm(&null_mode_check_timer);
    os_timer_setfn(&null_mode_check_timer, null_mode_check_timer_cb, false);
      //The function null_mode_check_timer_cb checks that the esp8266 has successfully changed the opmode to NULL_MODE prior to entering LIGHT_SLEEP
      //This callback doesn't need to be registered with SWTIMER_REG_CB since the timer will have terminated before entering LIGHT_SLEEP
    os_timer_arm(&null_mode_check_timer, 1, 1);
  }
  else{
    PMSLEEP_ERR("opmode change fail");
  }
  PMSLEEP_DBG("END");
  return;
}

#endif
