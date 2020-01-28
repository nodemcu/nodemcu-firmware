// Module for driving TRIAC-based dimmers
// This module implements phase-dimming for TRIAC-based mains dimmers.
// brief description on how TRIAC-based dimming works: https://www.lamps-on-line.com/leading-trailing-edge-led-dimmers

// Schematics for a homemade module: https://hackaday.io/project/165927-a-digital-ac-dimmer-using-arduino/details
// Example commercial hardware: https://robotdyn.com/ac-light-dimmer-module-1-channel-3-3v-5v-logic-ac-50-60hz-220v-110v.html
// These modules come with a TRIAC whose gate is driven by a GPIO pin, which is isolated
// from mains by an optocoupler. These modules also come with a zero-crossing detector,
// that raises a pin when the AC mains sine wave signal crosses 0V.

// Phase dimming is implemented in this module by dedicating CPU1 entirely to this purpose... Phase dimming
// requires very accurate timing. Configuring timer interrupts in the "busy" CPU0
// does not work properly, with FreeRTOS scheduler, WiFi and so on demanding their share of the CPU
// at random intervals, which would make dimmed lamps flicker.

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

// lua interface running on CPU0 communicates with the busy loop in CPU1 by means
// of a message structure defined here:

// message_type_t defines the two types of messages:
typedef enum {
    MT_IDLE = 0,    // indicates no message waiting to be read
    MT_ADD = 1,     // message contains information for a new pin to be controlled
    MT_REMOVE = 2,  // message indicates dimmer must stop controlling the given pin
} message_type_t;

typedef struct {
    message_type_t message_type;  // type of message, as defined above
    esp_err_t err;                // return value set by the CPU1 busy loop after processing the message
    union {
        int pin;     // used for MT_REMOVE, the pin to be removed from control
        dim_t* dim;  // used for MT_ADD; pointer to a dim_t containing info about the pin to be controlled and how
    };

} dimmer_message_t;

#define DIM_MODE_LEADING_EDGE 0x0
#define DIM_MODE_TRAILING_EDGE 0x1
#define STACK_SIZE 512  // Stack size for the CPU1 FreeRTOS task.

// Every mains cycle, the lamp brightness will gradually get closer to the desired brightness.
// this avoids damaging power supplies and rushing current through the dimmer module.
// DEFAULT_TRANSITION_SPEED controls the default fade speed.
// It is defined as a per mille (‰) brightness delta per half mains cycle (10ms if 50Hz)
// for example, if set to 20, a light would go from zero to full brightness in 1000 / 20 * 10ms = 500ms
// Set to 1000 to disable fading as default.
// This is a default. The user can define a specific value when calling dimmer.setup()
#define DEFAULT_TRANSITION_SPEED 100 /* 1-1000 */

// message is a shared variable among CPU0 (lua) and CPU1 (busy control loop)
// CPU0 fills the structure and CPU1 processes it
static volatile dimmer_message_t message = {.message_type = MT_IDLE};
static int zc_pin = -1;            // input pin to watch for zero crossing sync pulses
static uint32_t zc_timestamp = 0;  // measured in CPU cycles, contains the last time a zero crossing was seen.
static uint32_t mains_period = 0;  // estimated period of half a mains cycle, measured in CPU cycles.
static dim_t* dims_head = NULL;    // linked list head to a list of dimmer-controlled pins
static int transition_speed;       //configured transition speed (1-1000)
StaticTask_t xTaskBuffer;          // required by FreeRTOS to allocate a static task

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

// check_err translates a ESP error to a Lua error
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

// check_setup ensures dimmer.setup() has been called
static void check_setup(lua_State* L) {
    if (zc_pin == -1) {
        luaL_error(L, "dimmer module not initialized");
    }
}

// findPin searches the dimmer-controlled list of pins and returns
// a pointer to the found structure, or NULL otherwise.
static dim_t* IRAM_ATTR findPin(int pin) {
    for (dim_t* dim = dims_head; dim != NULL; dim = dim->next) {
        if (dim->pin == pin) {
            return dim;
        }
    }
    return NULL;
}

/**** the following code runs in CPU1 only ****/

// msg_add_dimmer processes a add dimmer message, updating the linked list.
static inline esp_err_t IRAM_ATTR msg_add_dimmer() {
    dim_t* dim = findPin(message.dim->pin);
    if (dim != NULL)
        return ESP_ERR_INVALID_ARG;

    message.dim->next = dims_head;
    dims_head = message.dim;
    message.dim = NULL;  // mark as NULL to avoid await_ack in CPU0 from freeing this memory
    return ESP_OK;
}

// msg_remove_dimmer processes a remove dimmer message, updating the linked list
static inline esp_err_t IRAM_ATTR msg_remove_dimmer() {
    dim_t* previous = NULL;
    for (dim_t* current = dims_head; current != NULL; current = current->next) {
        if (current->pin == message.pin) {
            message.dim = current;  // store the to-be-removed dim in the message so await_ack in CPU0 frees it.
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

// cpu1_loop is the entry point for the dimmer control busy loop
static void IRAM_ATTR cpu1_loop(void* parms) {
    // disable FreeRTOS scheduler tick timer for CPU1, since we only want this task running
    // and don't want the scheduler to get in the way and make our lights flicker
    portDISABLE_INTERRUPTS();
    ESP_INTR_DISABLE(XT_TIMER_INTNUM);
    portENABLE_INTERRUPTS();

    //avoid the whatchdog to reset our chip thinking some task is hanging the CPU
    rtc_wdt_protect_off();
    rtc_wdt_disable();

    // target specifies how many CPU cycles to wait before we poll GPIO to search for zero crossing
    uint32_t target = 0;

    // max_target determines a timeout waiting to register a zero crossing. It is calculated
    // as the max number of CPU cycles in half a mains cycle for 50Hz mains (worst case), increased by 10% for margin.
    uint32_t max_target = esp_clk_cpu_freq() / 50 /*Hz*/ / 2 /*half cycle*/ * 11 / 10 /* increase by 10%*/;

    while (true) {
        // check if there is a message waiting to be processed:
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
            message.message_type = MT_IDLE;  // this will release CPU0 waiting in await_ack()
        }

        // `now` contains the current value of the CPU cycle counter
        uint32_t now = xthal_get_ccount();
        // `elapsed` contains the number of CPU cycles since the last zero crossing
        uint32_t elapsed = now - zc_timestamp;

        // the following avoids polling GPIO unless we are close to a zero crossing:
        if ((elapsed > target) && (GPIO_INPUT_GET(zc_pin) == 1)) {
            // At this point, zero crossing has been detected.
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
            uint32_t levelStep = mains_period * transition_speed / 1000;

            for (dim_t* dim = dims_head; dim != NULL; dim = dim->next) {
                int32_t levelDiff = dim->target_level - dim->level;
                if (levelDiff > 0) {
                    dim->level += MIN(levelStep, levelDiff);
                } else if (levelDiff < 0) {
                    dim->level -= MIN(levelStep, -levelDiff);
                }
            }

            // `elapsed` now contains the measured duration of the period, so store it.
            // some dimmer modules keep the zc signal high when there is no mains input
            // The following sets period to 0 if it is too small, to signal there is no mains input.
            mains_period = (elapsed < 1000) ? 0 : elapsed;

            // remember when this zero crossing happened:
            zc_timestamp = now;

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

/**** the following code runs in CPU0 only ****/

// await_ack spin locks waiting for CPU1 to mark the message as processed.
static void await_ack(lua_State* L) {
    while (message.message_type != MT_IDLE)
        ;   // wait until CPU1 sets the message back to MT_IDLE.
            //should be a few milliseconds

    if (message.dim != NULL) {
        // Free dimmed pin memory in case an item was removed or we were trying to add an already
        // existing pin.
        luaM_freemem(L, message.dim, sizeof(dim_t));
        message.dim = NULL;
    }
    check_err(L, message.err);
}

// lua: dimmer.add(pin, mode): Adds a pin to the dimmer module.
// pin: the GPIO pin to control. Will configure the pin as output.
// mode: (optional) dimming mode, either dimmer.LEADING_EDGE (default) or dimmer.TRAILING_EDGE
// will throw an error if the pin was already added or an incorrect GPIO pin is provided.
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

    // the moment message_type is set, CPU1 will process it, so set this value the last one
    message.message_type = MT_ADD;
    await_ack(L);

    return 0;
}

// lua: dimmer.remove(pin): Removes a pin from the dimmer module's control
// pin: pin to remove
// will throw an error if the pin is not currently controlled by the module
static int dimmer_remove(lua_State* L) {
    check_setup(L);

    message.pin = luaL_checkint(L, 1);

    // the moment message_type is set, CPU1 will process it, so set this value the last one
    message.message_type = MT_REMOVE;
    await_ack(L);
    return 0;
}

// lua: dimmer.setLevel(pin, brightness): changes the brightness level for a dimmed pin
// pin: pin to configure
// brightness: per-mille brightness level, 0: off, 1000 fully on.
static int dimmer_setLevel(lua_State* L) {
    check_setup(L);

    int pin = luaL_checkint(L, 1);
    int brightness = luaL_checkint(L, 2);

    dim_t* dim = findPin(pin);
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

// lua: dimmer.mainsFrequency(): returns the measured mains frequency, in cHz
static int dimmer_mainsFrequency(lua_State* L) {
    int mainsF = 0;
    if (mains_period > 0) {
        mainsF = (int)((esp_clk_cpu_freq() * 50.0) / mains_period);
    }
    lua_pushinteger(L, mainsF);
    return 1;
}

// lua: dimmer.setup(zc_pin, transition_speed) : starts the dimmer module
// zc_pin: input pin connected to the zero-crossing detector
// transition_speed: 1-1000. Allows controlling the speed at which to change brightness.
// set to 1000 to disable fading altogether (instant change).
// defaults to a quick ~100ms fade.
static int dimmer_setup(lua_State* L) {
    if (zc_pin != -1) {
        return 0;  // already setup
    }

    zc_pin = luaL_checkint(L, 1);
    transition_speed = luaL_optint(L, 2, DEFAULT_TRANSITION_SPEED);

    // configure the zc pin as output
    check_err(L, gpio_set_direction(zc_pin, GPIO_MODE_INPUT));
    check_err(L, gpio_set_pull_mode(zc_pin, GPIO_PULLDOWN_ONLY));

    // launch the CPU1 busy loop task
    TaskHandle_t handle;
    BaseType_t result = xTaskCreatePinnedToCore(
        cpu1_loop,
        "dimmer",
        STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 20,
        &handle,
        1);  // core 1

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
LROT_FUNCENTRY(mainsFrequency, dimmer_mainsFrequency)
LROT_NUMENTRY(TRAILING_EDGE, DIM_MODE_TRAILING_EDGE)
LROT_NUMENTRY(LEADING_EDGE, DIM_MODE_LEADING_EDGE)
LROT_END(dimmer, NULL, 0)

int luaopen_dimmer(lua_State* L) {
    return 0;
}

NODEMCU_MODULE(DIMMER, "dimmer", dimmer, luaopen_dimmer);
