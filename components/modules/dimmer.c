// Module for interfacing with an MQTT broker
#include "driver/timer.h"
#include "esp_log.h"
#include "lauxlib.h"
#include "lmem.h"
#include "lnodeaux.h"
#include "module.h"
#include "platform.h"
#include "rom/gpio.h"
#include "task/task.h"

#include <string.h>

#define DIMMER_METATABLE "dimmer.mt"
#define TAG "DIMMER"

typedef struct {
    int pin;
    uint8_t mode;
    int level;
} dim_t;

static intr_handle_t s_timer_handle;
static int timerGroup;
static int timerNum;
static int t = 0;
//static bool reset = false;
static dim_t* dims = NULL;
static int dimCount = 0;

static timg_dev_t* timerGroupDev;

static void timer_isr(void* arg) {
    t++;
    //    static int io_state = 0;
    timerGroupDev->int_clr_timers.t0 = 1;
    timerGroupDev->hw_timer[timerNum].config.alarm_en = 1;

    //   io_state ^= 1;  //Toggle the pins state
    //   gpio_set_direction(5, 0x00000002);
    //   gpio_set_level(5, io_state);
}

/*
static void zc_isr(void* arg) {
}
*/

// pin
static int dimmer_add(lua_State* L) {
    int pin = luaL_checkint(L, 1);
    for (int i = 0; i < dimCount; i++) {
        if (dims[i].pin == pin) {
            return 0;
        }
    }
    dims = luaM_realloc_(L, dims, dimCount * sizeof(dim_t), (dimCount + 1) * sizeof(dim_t));
    dims[dimCount].pin = pin;
    dims[dimCount].level = 0;
    dims[dimCount].mode = 0;
    dimCount++;
    return 0;
}

static int dimmer_remove(lua_State* L) {
    int pin = luaL_checkint(L, 1);
    for (int i = 0; i < dimCount; i++) {
        if (dims[i].pin == pin) {
            for (int j = i; j < dimCount - 1; j++) {
                dims[j] = dims[j + 1];
            }
            dims = luaM_realloc_(L, dims, dimCount * sizeof(dim_t), (dimCount - 1) * sizeof(dim_t));
            dimCount--;
            return 0;
        }
    }
    luaL_error(L, "Error: pin %d is not dimmed.", pin);
    return 0;
}

static int dimmer_list_debug(lua_State* L) {
    for (int i = 0; i < dimCount; i++) {
        ESP_LOGD(TAG, "pin=%d, mode=%d, level=%d", dims[i].pin, dims[i].mode, dims[i].level);
    }
    return 0;
}

// hw timer group, hw_timer num, zc pin
static int dimmer_setup(lua_State* L) {
    timerGroup = luaL_checkint(L, 1);
    timerNum = luaL_checkint(L, 2);
    int zcPin = luaL_checkint(L, 3);
    ESP_LOGD(TAG, "Dimmer setup. TG=%d, TN=%d, ZC=%d", timerGroup, timerNum, zcPin);

    if (timerGroup == 0) {
        timerGroupDev = &TIMERG0;
    } else {
        timerGroupDev = &TIMERG1;
    }

    timer_config_t config = {
        .alarm_en = true,
        .counter_en = false,
        .intr_type = TIMER_INTR_LEVEL,
        .counter_dir = TIMER_COUNT_UP,
        .auto_reload = true,
        .divider = 4000 /* 50 us per tick */
    };

    timer_init(timerGroup, timerNum, &config);

    timer_set_counter_value(timerGroup, timerNum, 0);
    printf("timer_set_counter_value\n");

    timer_set_alarm_value(timerGroup, timerNum, 1000000);
    printf("timer_set_alarm_value\n");
    timer_enable_intr(timerGroup, timerNum);
    printf("timer_enable_intr\n");
    timer_isr_register(timerGroup, timerNum, &timer_isr, NULL, 0, &s_timer_handle);
    printf("timer_isr_register\n");

    timer_start(timerGroup, timerNum);
    printf("timer_start\n");

    return 0;
}

// map client methods to functions:
static const LUA_REG_TYPE dimmer_metatable_map[] =
    {
        /*
        {LSTRKEY("connect"), LFUNCVAL(mqtt_connect)},
        {LSTRKEY("close"), LFUNCVAL(mqtt_close)},
        {LSTRKEY("lwt"), LFUNCVAL(mqtt_lwt)},
        {LSTRKEY("publish"), LFUNCVAL(mqtt_publish)},
        {LSTRKEY("subscribe"), LFUNCVAL(mqtt_subscribe)},
        {LSTRKEY("unsubscribe"), LFUNCVAL(mqtt_unsubscribe)},
        {LSTRKEY("on"), LFUNCVAL(mqtt_on)},
        {LSTRKEY("__gc"), LFUNCVAL(mqtt_delete)},
        {LSTRKEY("__index"), LROVAL(mqtt_metatable_map)},
        */
        {LNILKEY, LNILVAL}};

// Module function map
static const LUA_REG_TYPE dimmer_map[] = {
    {LSTRKEY("setup"), LFUNCVAL(dimmer_setup)},
    {LSTRKEY("add"), LFUNCVAL(dimmer_add)},
    {LSTRKEY("remove"), LFUNCVAL(dimmer_remove)},
    {LSTRKEY("list"), LFUNCVAL(dimmer_list_debug)},
    {LNILKEY, LNILVAL}};

int luaopen_dimmer(lua_State* L) {
    luaL_rometatable(L, DIMMER_METATABLE, (void*)dimmer_metatable_map);  // create metatable for dimmer
    return 0;
}

NODEMCU_MODULE(DIMMER, "dimmer", dimmer_map, luaopen_dimmer);
