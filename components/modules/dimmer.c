// Module for interfacing with an MQTT broker
#include <string.h>
#include "driver/gpio.h"
#include "driver/timer.h"
#include "esp_clk.h"
#include "esp_ipc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/xtensa_timer.h"
#include "lauxlib.h"
#include "lmem.h"
#include "lnodeaux.h"
#include "module.h"
#include "platform.h"
#include "rom/gpio.h"
#include "soc/cpu.h"
#include "soc/rtc_wdt.h"

#define TAG "DIMMER"

typedef struct {
    int pin;
    int mode;
    uint32_t level;
    bool switched;
} dim_t;

typedef struct {
    esp_err_t err;
} config_interrupt_t;

#define DEBOUNCE_CYCLES 400000  // good for any CPU and mains frequency

#define DIM_TIMER0 0x0
#define DIM_TIMER1 0x1
#define DIM_TIMER2 0x2
#define DIM_TIMER3 0x3
#define DIM_MODE_LEADING_EDGE 0x0
#define DIM_MODE_TRAILING_EDGE 0x1

// menuconfig changes required in components/ESP32-specific...
// removed: "Also watch CPU1 tick interrupt"
// removed: "Watch CPU1 idle task"

static volatile bool disable_loop = false;
static volatile bool disable_loop_ack = false;
static volatile int zCount = 0;
static int zcPin = -1;
static volatile uint32_t zcTimestamp = 0;
static uint32_t p = 0;
static dim_t* dims = NULL;
static int dimCount = 0;

static int check_err(lua_State* L, esp_err_t err) {
    switch (err) {
        case ESP_ERR_INVALID_ARG:
            luaL_error(L, "invalid argument");
        case ESP_ERR_INVALID_STATE:
            luaL_error(L, "internal logic error");
        case ESP_OK:
            break;
    }
    return 0;
}

static void enable_interrupts() {
    disable_loop = false;
    disable_loop_ack = false;
}

static void disable_interrupts() {
    disable_loop = true;
    while (!disable_loop_ack)
        ;
}

// pin
static int dimmer_add(lua_State* L) {
    int pin = luaL_checkint(L, 1);
    int mode = luaL_optint(L, 2, DIM_MODE_LEADING_EDGE);
    for (int i = 0; i < dimCount; i++) {
        if (dims[i].pin == pin) {
            return 0;
        }
    }
    check_err(L, gpio_set_direction(pin, GPIO_MODE_OUTPUT));
    disable_interrupts();
    dims = luaM_realloc_(L, dims, dimCount * sizeof(dim_t), (dimCount + 1) * sizeof(dim_t));
    dims[dimCount].pin = pin;
    dims[dimCount].level = (mode == DIM_MODE_LEADING_EDGE) ? p * 2 : 0;
    dims[dimCount].mode = mode;
    dims[dimCount].switched = false;
    dimCount++;
    gpio_set_level(pin, 0);
    enable_interrupts();
    return 0;
}

static int dimmer_remove(lua_State* L) {
    int pin = luaL_checkint(L, 1);
    disable_interrupts();
    for (int i = 0; i < dimCount; i++) {
        if (dims[i].pin == pin) {
            for (int j = i; j < dimCount - 1; j++) {
                dims[j] = dims[j + 1];
            }
            dims = luaM_realloc_(L, dims, dimCount * sizeof(dim_t), (dimCount - 1) * sizeof(dim_t));
            dimCount--;
            enable_interrupts();
            return 0;
        }
    }
    enable_interrupts();
    luaL_error(L, "Error: pin %d is not dimmed.", pin);
    return 0;
}

static int dimmer_list_debug(lua_State* L) {
    ESP_LOGW(TAG, "p=%u, zcount=%d, zcTimestamp=%u, esp_freq=%d", p, zCount, zcTimestamp, esp_clk_cpu_freq());
    disable_interrupts();
    int dc = dimCount;
    dim_t dimsc[dimCount];
    memcpy(dimsc, dims, sizeof(dim_t) * dimCount);
    enable_interrupts();

    for (int i = 0; i < dc; i++) {
        ESP_LOGW(TAG, "pin=%d, mode=%d, level=%u", dimsc[i].pin, dimsc[i].mode, dimsc[i].level);
    }
    return 0;
}

static int dimmer_set_level(lua_State* L) {
    int pin = luaL_checkint(L, 1);

    for (int i = 0; i < dimCount; i++) {
        if (dims[i].pin == pin) {
            int level = luaL_checkint(L, 2);
            if (dims[i].mode == DIM_MODE_LEADING_EDGE) {
                level = 1000 - level;
            }
            if (level >= 1000) {
                dims[i].level = p * 2;  // ensure it will never switch.
            } else if (level <= 0) {
                dims[i].level = 0;
            } else {
                dims[i].level = (uint32_t)((((double)level) / 1000.0) * ((double)p));
            }
            return 0;
        }
    }

    luaL_error(L, "Cannot set dim level of unconfigured pin %d", pin);
    return 0;
}

static int dimmer_mainsFrequency(lua_State* L) {
    int mainsF = 0;
    if (p > 0) {
        mainsF = (int)((esp_clk_cpu_freq() * 50.0) / p);
    }
    lua_pushinteger(L, mainsF);
    return 1;
}

static void IRAM_ATTR disable_timer() {
    /*
    Enable the timer interrupt at the device level. Don't write directly
    to the INTENABLE register because it may be virtualized.
    */
    /*
#ifdef __XTENSA_CALL0_ABI__
    asm volatile(
        "movi    a2, XT_TIMER_INTEN\n")
        "call0   xt_ints_off")
#else
    asm volatile(
        "movi a6, XT_TIMER_INTEN\n"
        "call4 xt_ints_off")
#endif
*/
    portDISABLE_INTERRUPTS();
    ESP_INTR_DISABLE(XT_TIMER_INTNUM);
    portENABLE_INTERRUPTS();
}

static void IRAM_ATTR stuff(void* parms) {
    //Find a way to disable the cpu1 tick interrupt.
    disable_timer();
    rtc_wdt_protect_off();
    rtc_wdt_disable();

    /*
    int level = 0;
    gpio_set_direction(16, GPIO_MODE_OUTPUT);
    while (1) {
        GPIO_OUTPUT_SET(16, level);
        if (level == 0) {
            level = 1;
        } else {
            level = 0;
        }
        delayMicroseconds(1000000);
    }
    */
    uint32_t volatile now;
    uint32_t volatile elapsed;
    while (true) {
        if (disable_loop) {
            disable_loop_ack = true;
            while (!disable_loop)
                ;
        }
        // xthal_get_ccount();
        RSR(CCOUNT, now)
        elapsed = now - zcTimestamp;
        if ((elapsed > 600000) && (GPIO_INPUT_GET(zcPin) == 1)) {
            zCount++;
            for (int i = 0; i < dimCount; i++) {
                if (dims[i].level == 0) {
                    GPIO_OUTPUT_SET(dims[i].pin, (1 - dims[i].mode));
                    dims[i].switched = true;
                } else {
                    GPIO_OUTPUT_SET(dims[i].pin, dims[i].mode);
                    dims[i].switched = false;
                }
            }
            zcTimestamp = now;
            p = elapsed;
        } else {
            for (int i = 0; i < dimCount; i++) {
                if ((dims[i].switched == false) && (elapsed > dims[i].level)) {
                    GPIO_OUTPUT_SET(dims[i].pin, (1 - dims[i].mode));
                    dims[i].switched = true;
                }
            }
        }
    }
}

#define STACK_SIZE 4096

StaticTask_t xTaskBuffer;

// hw timer group, hw_timer num, zc pin
static int dimmer_setup(lua_State* L) {
    zcPin = luaL_checkinteger(L, -1);

    check_err(L, gpio_set_direction(zcPin, GPIO_MODE_INPUT));
    check_err(L, gpio_set_pull_mode(zcPin, GPIO_PULLUP_ONLY));

    ESP_LOGD(TAG, "Dimmer setup. ZC=%d", zcPin);
    TaskHandle_t handle;
    xTaskCreatePinnedToCore(
        stuff,       // Function that implements the task.
        "stuff",     // Text name for the task.
        STACK_SIZE,  // Stack size in bytes, not words.
        (void*)1,    // Parameter passed into the task.
        tskIDLE_PRIORITY + 20,
        &handle,
        //        xStack,        // Array to use as the task's stack.
        //        &xTaskBuffer,  // Variable to hold the task's data structure.
        1);

    return 0;
}

// Module function map
LROT_BEGIN(dimmer)
LROT_FUNCENTRY(setup, dimmer_setup)
LROT_FUNCENTRY(add, dimmer_add)
LROT_FUNCENTRY(remove, dimmer_remove)
LROT_FUNCENTRY(setLevel, dimmer_set_level)
LROT_FUNCENTRY(list, dimmer_list_debug)
LROT_FUNCENTRY(mainsFrequency, dimmer_mainsFrequency)
LROT_NUMENTRY(TIMER_0, DIM_TIMER0)
LROT_NUMENTRY(TIMER_1, DIM_TIMER1)
LROT_NUMENTRY(TIMER_2, DIM_TIMER2)
LROT_NUMENTRY(TIMER_3, DIM_TIMER3)

LROT_END(dimmer, NULL, 0)

int luaopen_dimmer(lua_State* L) {
    //luaL_rometatable(L, DIMMER_METATABLE, (void*)dimmer_metatable_map);  // create metatable for dimmer
    return 0;
}

NODEMCU_MODULE(DIMMER, "dimmer", dimmer, luaopen_dimmer);
