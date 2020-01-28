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
#include "lmem.h"
#include "lnodeaux.h"
#include "module.h"
#include "rom/gpio.h"
#include "soc/rtc_wdt.h"

// TAG for debug logging
#define TAG "DIMMER"

// dim_t defines a dimmer pin configuration
typedef struct dim_t {
    int pin;   // pin to apply phase-dimming to.
    int mode;  // dimming mode: LEADING_EDGE or TRAILING_EDGE
    // level indicates the current brightness level, computed as the number of
    // CPU cycles to wait since the last zero crossing until switching the signal on or off.
    uint32_t level;
    // target_level indicates the desired brightness level, computed in CPU cycles, as above.
    // every mains cycle, `level` will get closer to `target_level`, so brightness changes gradually.
    // this avoids damaging power supplies and rushing current through the dimmer module.
    uint32_t target_level;
    bool switched;  //wether this output has already been switched on or off in the current mains cycle.
    struct dim_t* next;
} dim_t;

typedef enum {
    MT_IDLE = 0,
    MT_ADD = 1,
    MT_REMOVE = 2,
    MT_SETLEVEL = 3,
} message_type_t;

typedef struct {
    message_type_t message_type;
    esp_err_t err;
    union {
        int pin;
        dim_t* dim;
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

static volatile dimmer_message_t message = {.message_type = MT_IDLE};
static volatile int zc_count = 0;
static int zc_pin = -1;
static uint32_t zc_timestamp = 0;
static uint32_t mains_period = 0;
static dim_t* dims_head = NULL;
static int transition_speed;
StaticTask_t xTaskBuffer;

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static void check_err(lua_State* L, esp_err_t err) {
    switch (err) {
        case ESP_ERR_INVALID_ARG:
            luaL_error(L, "invalid argument or gpio pin");
        case ESP_ERR_INVALID_STATE:
            luaL_error(L, "internal logic error");
        case ESP_OK:
            break;
        default:
            luaL_error(L, "Error code %d", err);
    }
}

static void check_setup(lua_State* L) {
    if (zc_pin == -1) {
        luaL_error(L, "dimmer module not initialized");
    }
}

static dim_t* find(int pin) {
    for (dim_t* dim = dims_head; dim != NULL; dim = dim->next) {
        if (dim->pin == pin) {
            return dim;
        }
    }
    return NULL;
}

static inline esp_err_t IRAM_ATTR msg_add_dimmer() {
    dim_t* dim = find(message.dim->pin);
    if (dim != NULL)
        return ESP_ERR_INVALID_ARG;

    message.dim->next = dims_head;
    dims_head = message.dim;
    message.dim = NULL;
    return ESP_OK;
}

static inline esp_err_t IRAM_ATTR msg_remove_dimmer() {
    dim_t* previous = NULL;
    for (dim_t* current = dims_head; current != NULL; current = current->next) {
        if (current->pin == message.pin) {
            message.dim = current;
            if (previous == NULL) {
                dims_head = dims_head->next;
            } else {
                previous->next = current->next;
            }
            return ESP_OK;
        }
        previous = current;
    }
    message.dim = NULL;
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
        if (message.message_type != MT_IDLE) {
            switch (message.message_type) {
                case MT_ADD:
                    message.err = msg_add_dimmer();
                    break;
                case MT_REMOVE:
                    message.err = msg_remove_dimmer();
                    break;
                default:
                    break;
            }
            message.message_type = MT_IDLE;
        }

        // now contains the current value of the CPU cycle counter
        uint32_t now = xthal_get_ccount();
        // elapsed contains the number of CPU cycles since the last zero crossing
        uint32_t elapsed = now - zc_timestamp;

        // the following avoids polling GPIO unless we are close to a zero crossing:
        if ((elapsed > target) && (GPIO_INPUT_GET(zc_pin) == 1)) {
            // Zero crossing has been detected.
            // Reset dimmers according to their mode, Leading edge or trailing edge
            for (dim_t* dim = dims_head; dim != NULL; dim = dim->next) {
                if (dim->level == 0) {
                    GPIO_OUTPUT_SET(dim->pin, (1 - dim->mode));
                    dim->switched = true;
                } else {
                    GPIO_OUTPUT_SET(dim->pin, dim->mode);
                    dim->switched = false;
                }
            }

            // For this cycle that starts, adjust the brightness level one step
            // closer to the target brightness level.
            // tempting to combine the below loop into the above one, but we want
            // to make sure pins are reset ASAP.
            zc_count++;
            zc_timestamp = now;
            uint32_t levelStep = mains_period * transition_speed / 1000;

            for (dim_t* dim = dims_head; dim != NULL; dim = dim->next) {
                int32_t levelDiff = dim->target_level - dim->level;
                if (levelDiff > 0) {
                    dim->level += MIN(levelStep, levelDiff);
                } else if (levelDiff < 0) {
                    dim->level -= MIN(levelStep, -levelDiff);
                }
            }

            // some dimmer modules keep the zc signal high when there is no mains input
            // The following sets period to 0, to signal there is no mains input.
            mains_period = (elapsed < 1000) ? 0 : elapsed;

            // Calibrate new target as 90% of the elapsed time.
            target = 9 * elapsed / 10;

        } else {
            // Check if it is time to turn on or off any dimmed output:
            for (dim_t* dim = dims_head; dim != NULL; dim = dim->next) {
                if ((dim->switched == false) && (elapsed > dim->level)) {
                    GPIO_OUTPUT_SET(dim->pin, (1 - dim->mode));
                    dim->switched = true;
                }
            }
        }
        // if too much time has pased since we detected a zero crossing, set our target to 0 to resync.
        // This could happen if mains power is cut.
        if (elapsed > max_target) {
            target = 0;
            mains_period = 0;  // mark period as 0 (no mains detected)
        }
    }
}

static void await_ack(lua_State* L) {
    while (message.message_type != MT_IDLE)
        ;
    if (message.dim != NULL) {
        luaM_freemem(L, message.dim, sizeof(dim_t));
        message.dim = NULL;
    }
    check_err(L, message.err);
}

// pin
static int dimmer_add(lua_State* L) {
    check_setup(L);

    int pin = luaL_checkint(L, 1);
    int mode = luaL_optint(L, 2, DIM_MODE_LEADING_EDGE);

    check_err(L, gpio_set_direction(pin, GPIO_MODE_OUTPUT));
    gpio_set_level(pin, 0);

    dim_t* dim = message.dim = (dim_t*)luaM_malloc(L, sizeof(dim_t));
    dim->pin = pin;
    dim->mode = mode;
    dim->next = NULL;
    dim->target_level = dim->level = (mode == DIM_MODE_LEADING_EDGE) ? mains_period * 2 : 0;
    dim->switched = false;

    message.message_type = MT_ADD;
    await_ack(L);

    return 0;
}

static int dimmer_remove(lua_State* L) {
    check_setup(L);

    message.pin = luaL_checkint(L, 1);

    message.message_type = MT_REMOVE;
    await_ack(L);
    return 0;
}

static int dimmer_setLevel(lua_State* L) {
    check_setup(L);

    int pin = luaL_checkint(L, 1);         // pin to configure
    int brightness = luaL_checkint(L, 2);  // brightness level (0-1000)

    dim_t* dim = find(pin);
    if (dim == NULL) {
        luaL_error(L, "invalid pin");
        return 0;
    }

    // translate brightness level 0-1000 to the number of CPU cycles signal must be on or off
    // after a mains zero crossing
    if (dim->mode == DIM_MODE_LEADING_EDGE) {  // invert value for leading edge dimmers
        brightness = 1000 - brightness;
    }
    if (brightness >= 1000) {
        // put a big value to ensure it will never switch. (keep on at all times)
        dim->target_level = mains_period << 1;
    } else if (brightness <= 0) {
        dim->target_level = 0;
    } else {
        // set target level as a per-mille of the total mains period, measured in CPU cycles.
        dim->target_level = (uint32_t)((((double)brightness) / 1000.0) * ((double)mains_period));
    }

    return 0;
}

static int dimmer_mainsFrequency(lua_State* L) {
    int mainsF = 0;
    if (mains_period > 0) {
        mainsF = (int)((esp_clk_cpu_freq() * 50.0) / mains_period);
    }
    lua_pushinteger(L, mainsF);
    return 1;
}

static int dimmer_list_debug(lua_State* L) {
    ESP_LOGW(TAG, "mains_period=%u, zc_count=%d, zc_timestamp=%u, esp_freq=%d", mains_period, zc_count, zc_timestamp, esp_clk_cpu_freq());

    for (dim_t* dim = dims_head; dim != NULL; dim = dim->next) {
        ESP_LOGW(TAG, "pin=%d, mode=%d, level=%u", dim->pin, dim->mode, dim->level);
    }
    return 0;
}

// dimmer.setup(zc_pin, transition_speed)
// zc_pin: input pin connected to the zero-crossing detector
// transition_speed: 1-1000. Allows controlling the speed at which to change brightness.
// set to 1000 to disable fading altogether (instant change).
// defaults to a quick ~100ms fade.
static int dimmer_setup(lua_State* L) {
    if (zc_pin != -1) {
        return 0;
    }
    zc_pin = luaL_checkint(L, 1);
    transition_speed = luaL_optint(L, 2, DEFAULT_TRANSITION_SPEED);

    check_err(L, gpio_set_direction(zc_pin, GPIO_MODE_INPUT));
    check_err(L, gpio_set_pull_mode(zc_pin, GPIO_PULLDOWN_ONLY));

    TaskHandle_t handle;
    BaseType_t result = xTaskCreatePinnedToCore(
        cpu1_loop,
        "dimmer",
        STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 20,
        &handle,
        1);

    if (result != pdPASS) {
        luaL_error(L, "Error starting dimmer task in CPU1");
        zc_pin = -1;
    }

    return 0;
}

// Module function map
LROT_BEGIN(dimmer)
LROT_FUNCENTRY(setup, dimmer_setup)
LROT_FUNCENTRY(add, dimmer_add)
LROT_FUNCENTRY(remove, dimmer_remove)
LROT_FUNCENTRY(setLevel, dimmer_setLevel)
LROT_FUNCENTRY(list, dimmer_list_debug)
LROT_FUNCENTRY(mainsFrequency, dimmer_mainsFrequency)
LROT_NUMENTRY(TRAILING_EDGE, DIM_MODE_TRAILING_EDGE)
LROT_NUMENTRY(LEADING_EDGE, DIM_MODE_LEADING_EDGE)
LROT_END(dimmer, NULL, 0)

int luaopen_dimmer(lua_State* L) {
    return 0;
}

NODEMCU_MODULE(DIMMER, "dimmer", dimmer, luaopen_dimmer);
