// Module for driving TRIAC-based dimmers
// This module implements phase-dimming for TRIAC-based mains dimmers.
// brief description on how TRIAC-based dimming works: https://www.lamps-on-line.com/leading-trailing-edge-led-dimmers

// Schematics for a homemade module: https://hackaday.io/project/165927-a-digital-ac-dimmer-using-arduino/details
// Example commercial hardware: https://robotdyn.com/ac-light-dimmer-module-1-channel-3-3v-5v-logic-ac-50-60hz-220v-110v.html
// These modules come with a TRIAC whose gate is driven by a GPIO pin, which is isolated
// from mains by an optocoupler. These modules also come with a zero-crossing detector,
// that raises a pin when the AC mains sine wave signal crosses 0V.

// Phase dimming is implemented by dedicating CPU1 entirely to this purpose... Phase dimming
// requires very accurate timing. Configuring timer interrupts in the "busy" CPU0
// does not cut it, with FreeRTOS scheduler, WiFi and so on demanding their share of the CPU
// at random times, which would make dimmed lamps flicker.

// Once the dimmer module is started, by means of dimmer.setup(), a busy loop is launched
// on CPU1 that monitors zero-crossing signals from the dimmer and turns on/off
// the TRIAC at the appropriate time, with nanosecond precision.

// To use this module, change the following in menuconfig:
// * Enable FreeRTOS in both cores by unselecting "Component Config/FreeRTOS/Run FreeRTOS only on first core"
// * Unselect  "ComponentConfig/ESP32-specific/Also watch CPU1 tick interrupt"
// * Unselect  "ComponentConfig/ESP32-specific/Watch CPU1 idle task"

// In your program, call dimmer.setup(zc_pin), indicating the pin input where you connected the zero crossing detector.
// Use dimmer.add(pin, type) to have the module control that pin.
// Type can be dimmer.LEADING_EDGE (default) or dimmer.TRAILING_EDGE, depending on the type of load/lightbulb.
// User dimmer.setLevel(pin, value) to set the desired brightness level. Value can be from 0 (off) to 1000 (fully on).
// Use dimmer.remove(pin) to have the dimmer module stop controlling that pin.

#include <string.h>
#include "driver/gpio.h"
#include "esp_clk.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/xtensa_timer.h"
#include "lauxlib.h"
#include "lnodeaux.h"
#include "module.h"
#include "rom/gpio.h"
#include "soc/rtc_wdt.h"

// TAG for debug logging
#define TAG "DIMMER"

// dim_t defines a dimmer pin configuration
typedef struct {
    int pin;   // pin to apply phase-dimming to.
    int mode;  // dimming mode: LEADING_EDGE or TRAILING_EDGE
    // level indicates the current brightness level, computed as the number of
    // CPU cycles to wait since the last zero crossing until switching the signal on or off.
    uint32_t level;
    // targetLevel indicates the desired brightness level, computed in CPU cycles, as above.
    // every mains cycle, `level` will get closer to `targetLevel`, so brightness changes gradually.
    // this avoids damaging power supplies and rushing current through the dimmer module.
    uint32_t targetLevel;
    bool switched;  //wether this output has already been switched on or off in the current mains cycle.
} dim_t;

typedef enum {
    MT_IDLE = 0,
    MT_ADD = 1,
    MT_REMOVE = 2,
    MT_SETLEVEL = 3,
} message_type_t;

typedef struct {
    message_type_t messageType;
    int pin;
    union {
        int mode;
        uint32_t targetLevel;
        esp_err_t err;
    };

} dimmer_message_t;

#define DIM_MODE_LEADING_EDGE 0x0
#define DIM_MODE_TRAILING_EDGE 0x1
#define STACK_SIZE 512

// Every mains cycle, the lamp brightness will gradually get closer to the desired brightness.
// this avoids damaging power supplies and rushing current through the dimmer module.
// DEFAULT_TRANSITION_SPEED controls the default fade speed.
// It is defined as a per mille (‰) brightness delta per half mains cycle (10ms if 50Hz)
// for example, if set to 20, a light would go from zero to full brightness in 1000 / 20 * 10ms = 500ms
// Set to 1000 to disable fading as default.
// This is a default. The user can define a specific value when calling dimmer.setup()
#define DEFAULT_TRANSITION_SPEED 100 /* 1-1000 */

static volatile dimmer_message_t message = {.messageType = MT_IDLE};
static volatile int zCount = 0;
static int zcPin = -1;
static volatile uint32_t zcTimestamp = 0;
static uint32_t p = 0;
static dim_t* dims = NULL;
static int dimCount = 0;
static int transitionSpeed;
StaticTask_t xTaskBuffer;

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static void check_err(lua_State* L, esp_err_t err) {
    switch (err) {
        case ESP_ERR_INVALID_ARG:
            luaL_error(L, "invalid argument or gpio pin");
        case ESP_ERR_INVALID_STATE:
            luaL_error(L, "internal logic error");
        case ESP_ERR_NO_MEM:
            luaL_error(L, "out of memory");
        case ESP_OK:
            break;
        default:
            luaL_error(L, "Error code %d", err);
    }
}

static void check_setup(lua_State* L) {
    if (zcPin == -1) {
        luaL_error(L, "dimmer module not initialized");
    }
}

static inline esp_err_t IRAM_ATTR msg_add_dimmer() {
    for (int i = 0; i < dimCount; i++) {
        if (dims[i].pin == message.pin) {
            return ESP_OK;
        }
    }
    dims = realloc(dims, (dimCount + 1) * sizeof(dim_t));
    if (dims == NULL) {
        return ESP_ERR_NO_MEM;
    }
    dims[dimCount].pin = message.pin;
    dims[dimCount].targetLevel = dims[dimCount].level = (message.mode == DIM_MODE_LEADING_EDGE) ? p * 2 : 0;
    dims[dimCount].mode = message.mode;
    dims[dimCount].switched = false;
    dimCount++;
    return ESP_OK;
}

static inline esp_err_t IRAM_ATTR msg_remove_dimmer() {
    for (int i = 0; i < dimCount; i++) {
        if (dims[i].pin == message.pin) {
            for (int j = i; j < dimCount - 1; j++) {
                dims[j] = dims[j + 1];
            }
            dims = realloc(dims, (dimCount - 1) * sizeof(dim_t));
            dimCount--;
            return ESP_OK;
        }
    }
    return ESP_ERR_INVALID_ARG;
}

static inline esp_err_t IRAM_ATTR msg_set_level() {
    for (int i = 0; i < dimCount; i++) {
        if (dims[i].pin == message.pin) {
            int targetLevel = message.targetLevel;
            if (dims[i].mode == DIM_MODE_LEADING_EDGE) {
                targetLevel = 1000 - targetLevel;
            }
            if (targetLevel >= 1000) {
                dims[i].targetLevel = p * 2;  // ensure it will never switch.
            } else if (targetLevel <= 0) {
                dims[i].targetLevel = 0;
            } else {
                dims[i].targetLevel = (uint32_t)((((double)targetLevel) / 1000.0) * ((double)p));
            }
            return ESP_OK;
        }
    }
    return ESP_ERR_INVALID_ARG;
}

static void IRAM_ATTR cpu1_loop(void* parms) {
    portDISABLE_INTERRUPTS();
    ESP_INTR_DISABLE(XT_TIMER_INTNUM);
    portENABLE_INTERRUPTS();
    rtc_wdt_protect_off();
    rtc_wdt_disable();

    // max_target determines a timeout waiting to register a zero crossing. It is calculated
    // as the max number of CPU cycles in half a mains cycle for 50Hz mains (worst case), increased by 10% for margin.
    uint32_t max_target = esp_clk_cpu_freq() / 50 /*Hz*/ / 2 /*half cycle*/ * 11 / 10 /* increase by 10%*/;

    // target specifies how many CPU cycles to wait before we poll GPIO to search for zero crossing
    uint32_t target = 0;

    while (true) {
        if (message.messageType != MT_IDLE) {
            switch (message.messageType) {
                case MT_ADD:
                    message.err = msg_add_dimmer();
                    break;
                case MT_REMOVE:
                    message.err = msg_remove_dimmer();
                    break;
                case MT_SETLEVEL:
                    message.err = msg_set_level();
                    break;
                default:
                    break;
            }
            message.messageType = MT_IDLE;
        }

        // now contains the current value of the CPU cycle counter
        uint32_t volatile now = xthal_get_ccount();
        // elapsed contains the number of CPU cycles since the last zero crossing
        uint32_t volatile elapsed = now - zcTimestamp;

        // the following avoids polling GPIO unless we are close to a zero crossing:
        if ((elapsed > target) && (GPIO_INPUT_GET(zcPin) == 1)) {
            // Zero crossing has been detected.
            zCount++;
            p = (elapsed < 1000) ? 0 : elapsed;
            uint32_t levelStep = p * transitionSpeed / 1000;

            for (int i = 0; i < dimCount; i++) {
                dim_t* dim = &dims[i];
                //Reset dimmers according to their mode
                // (Leading edge or trailing edge)
                if (dim->level == 0) {
                    GPIO_OUTPUT_SET(dim->pin, (1 - dim->mode));
                    dim->switched = true;
                } else {
                    GPIO_OUTPUT_SET(dim->pin, dim->mode);
                    dim->switched = false;
                }
                // For this cycle that starts, adjust the brightness level one step
                // closer to the target brightness level.
                int32_t levelDiff = dim->targetLevel - dim->level;
                if (levelDiff > 0) {
                    dim->level += MIN(levelStep, levelDiff);
                } else if (levelDiff < 0) {
                    dim->level -= MIN(levelStep, -levelDiff);
                }
            }
            zcTimestamp = now;
            // some dimmer modules keep the signal high when there is no mains input
            // The following sets period to 0, to signal there is no mains input.

            // Calibrate new target as 90% of the elapsed time.
            target = 9 * elapsed / 10;
        } else {
            // Check if it is time to turn on or off any dimmed output:
            for (int i = 0; i < dimCount; i++) {
                if ((dims[i].switched == false) && (elapsed > dims[i].level)) {
                    GPIO_OUTPUT_SET(dims[i].pin, (1 - dims[i].mode));
                    dims[i].switched = true;
                }
            }
        }
        // if too much time has pased since we detected a zero crossing, set our target to 0 to resync.
        // This could happen if mains power is cut.
        if (elapsed > max_target) {
            target = 0;
            p = 0;  // mark period as 0 (no mains detected)
        }
    }
}

static void awaitAck(lua_State* L) {
    while (message.messageType != MT_IDLE)
        ;
    check_err(L, message.err);
}

// pin
static int dimmer_add(lua_State* L) {
    check_setup(L);
    message.pin = luaL_checkint(L, 1);
    message.mode = luaL_optint(L, 2, DIM_MODE_LEADING_EDGE);
    check_err(L, gpio_set_direction(message.pin, GPIO_MODE_OUTPUT));
    gpio_set_level(message.pin, 0);

    message.messageType = MT_ADD;
    awaitAck(L);

    return 0;
}

static int dimmer_remove(lua_State* L) {
    check_setup(L);
    message.pin = luaL_checkint(L, 1);
    message.messageType = MT_REMOVE;
    awaitAck(L);
    return 0;
}

static int dimmer_set_level(lua_State* L) {
    check_setup(L);
    message.pin = luaL_checkint(L, 1);
    message.targetLevel = luaL_checkint(L, 2);
    message.messageType = MT_SETLEVEL;
    awaitAck(L);

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

static int dimmer_list_debug(lua_State* L) {
    ESP_LOGW(TAG, "p=%u, zcount=%d, zcTimestamp=%u, esp_freq=%d", p, zCount, zcTimestamp, esp_clk_cpu_freq());

    for (int i = 0; i < dimCount; i++) {
        ESP_LOGW(TAG, "pin=%d, mode=%d, level=%u", dims[i].pin, dims[i].mode, dims[i].level);
    }
    return 0;
}

// dimmer.setup(zcPin, transitionSpeed)
// zcPin: input pin connected to the zero-crossing detector
// transitionSpeed: 1-1000. Allows controlling the speed at which to change brightness.
// set to 1000 to disable fading altogether (instant change).
// defaults to a quick ~100ms fade.
static int dimmer_setup(lua_State* L) {
    if (zcPin != -1) {
        return 0;
    }
    zcPin = luaL_checkint(L, 1);
    transitionSpeed = luaL_optint(L, 2, DEFAULT_TRANSITION_SPEED);

    check_err(L, gpio_set_direction(zcPin, GPIO_MODE_INPUT));
    check_err(L, gpio_set_pull_mode(zcPin, GPIO_PULLDOWN_ONLY));

    TaskHandle_t handle;
    BaseType_t result = xTaskCreatePinnedToCore(
        cpu1_loop,
        "cpu1 loop",
        STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 20,
        &handle,
        1);

    if (result != pdPASS) {
        luaL_error(L, "Error starting dimmer task in CPU1");
        zcPin = -1;
    }

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
LROT_NUMENTRY(TRAILING_EDGE, DIM_MODE_TRAILING_EDGE)
LROT_NUMENTRY(LEADING_EDGE, DIM_MODE_LEADING_EDGE)
LROT_END(dimmer, NULL, 0)

int luaopen_dimmer(lua_State* L) {
    return 0;
}

NODEMCU_MODULE(DIMMER, "dimmer", dimmer, luaopen_dimmer);
