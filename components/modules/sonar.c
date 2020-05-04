// Module sonar drives the typical ultrasound based sonars, such as the HC-SR04
// these sensors are activated by a "trig" pin that must be held high for a few microseconds
// An ultrasound is emitted and when an echo is received, the sensor sends a pulse as long
// as the time it took for the echo to come back. Distance can be then calculated by multiplying
// the travel time by the speed of sound.
//
// To use this module, choose two GPIO pins and connect them to HC-SR04 TRIG and ECHO pins.
// HC-SR04 requires 5V Vcc!. The echo pin must be connected to a voltage divider to convert the
// echo pulse to 3.3V to avoid damage to your ESP32
//
// In Lua, call sonarobj=sonar.new(trig, echo), indicate the GPIO pins.
// You can then call sonarobj:ping(callback) which will send out a sonar ping and invoke your callback
// with the distance in millimeters.
//
// IMPLEMENTATION DETAILS:
//
// This module uses the RMT infrared driver hardware present in ESP32 for accurate pulse measurement.
// Each sonar object takes two RMT channels, one for sending and another for receiving. There are 8 RMT channels
// so up to 4 sonars can be driven simultaneously. Code could be optimized to share these channels in the future.
//
// When instantiating a new sonar object with sonar.new() a RTOS task is created (ping_task).
// This task will wait for ping requests by means of a RTOS message queue.
// When a ping is requested, it will send out a ping pulse to the sensor and await the response.
// When the response is received, a message is posted to a NodeMCU event task, which will in turn invoke
// the provided Lua callback.

#include <stdio.h>
#include <string.h>

#include "driver/rmt.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lauxlib.h"
#include "lnodeaux.h"
#include "module.h"
#include "platform_rmt.h"
#include "task/task.h"

#define SONAR_METATABLE "sonar.mt"    // name of the metatable to register in Lua for the sonar object
#define TAG "sonar"                   // log tag
#define SPEED_OF_SOUND 340            // speed of sound in m/s
#define DISTANCE_OUT_OF_RANGE 999999  // value that will be returned when the sensor does not get an echo
#define HCSR04_MAX_TIMEOUT_US 25000   /*!< RMT receiver timeout value(us) */

// ticks to time conversion constants
#define RMT_CLK_DIV 100                                  /*!< RMT counter clock divider */
#define RMT_TICK_10_US (80000000 / RMT_CLK_DIV / 100000) /*!< RMT counter value for 10 us.(Source clock is APB clock) */

// ticks <-> time conversion macros
#define US2TICKS(us) (us / 10 * RMT_TICK_10_US)
#define TICKS2US(ticks) (ticks * 10 / RMT_TICK_10_US)

// sonar_context_t is the main object that keeps the state of the sonar object userdata
typedef struct {
    rmt_channel_t tx;         // RMT trig channel
    rmt_channel_t rx;         // RMT echo channel
    QueueHandle_t queue;      // message queue
    TaskHandle_t taskHandle;  // RTOS ping_task in charge of executing the pings
} sonar_context_t;

// sonar_message_type_t is a list of possible messages that can be sent to the ping_task
typedef enum {
    SONAR_PING,    // ping requested
    SONAR_REMOVE,  // end the task
} sonar_message_type_t;

// sonar_message_t contains the necessary information to request an action from the ping_task
typedef struct {
    sonar_message_type_t message_type;  // action requested, as described in sonar_message_type_t
    lua_ref_t callback;                 // Lua callback to invoke for SONAR_PING messages to return the measured distance
} sonar_message_t;

// callback_message_t contains the necessary information to dispatch a Lua callback
typedef struct {
    int32_t distance;    // measured distance
    lua_ref_t callback;  // Lua callback to invoke
} callback_message_t;

// sonar_tx_init initializes RMT hardware to use as a trigger signal transmitter

static esp_err_t sonar_tx_init(rmt_channel_t tx_channel, gpio_num_t gpio) {
    rmt_config_t rmt_tx;
    rmt_tx.channel = tx_channel;
    rmt_tx.gpio_num = gpio;
    rmt_tx.mem_block_num = 1;
    rmt_tx.clk_div = RMT_CLK_DIV;
    rmt_tx.tx_config.loop_en = false;

    rmt_tx.tx_config.carrier_duty_percent = 50;
    rmt_tx.tx_config.carrier_freq_hz = 38000;
    rmt_tx.tx_config.carrier_level = 1;
    rmt_tx.tx_config.carrier_en = 0;  // off

    rmt_tx.tx_config.idle_level = 0;
    rmt_tx.tx_config.idle_output_en = true;
    rmt_tx.rmt_mode = RMT_MODE_TX;
    esp_err_t err = rmt_config(&rmt_tx);
    if (err != ESP_OK)
        return err;
    return rmt_driver_install(rmt_tx.channel, 0, 0);
}

// sonar_rx_init initializes RMT hardware to use as an echo signal receiver
static esp_err_t sonar_rx_init(rmt_channel_t rx_channel, gpio_num_t gpio) {
    rmt_config_t rmt_rx;
    rmt_rx.channel = rx_channel;
    rmt_rx.gpio_num = gpio;
    rmt_rx.clk_div = RMT_CLK_DIV;
    rmt_rx.mem_block_num = 1;
    rmt_rx.rmt_mode = RMT_MODE_RX;
    rmt_rx.rx_config.filter_en = false;
    rmt_rx.rx_config.filter_ticks_thresh = 100;
    rmt_rx.rx_config.idle_threshold = US2TICKS(HCSR04_MAX_TIMEOUT_US);
    esp_err_t err = rmt_config(&rmt_rx);
    if (err != ESP_OK)
        return err;
    return rmt_driver_install(rmt_rx.channel, 1000, 0);
}

task_handle_t dispatch_callback_h;  // contains a NodeMCU task event handler pointing to dispatch_callback below

// dispatch_callback runs in the context of NodeMCU message queue
// it takes the received message which contains a reference to a Lua callback and
// invokes it.
static void dispatch_callback(task_param_t param, task_prio_t prio) {
    // param is actually a pointer to our callback message, so cast it back.
    callback_message_t* callback_message_p = (callback_message_t*)param;
    // make a copy so we can dispose the dynamic memory in which the message came:
    callback_message_t callback_message = *callback_message_p;

    // free the message memory that was dynamically allocated in ping_task(). We do this now
    // to make sure we don't leak memory if something below fails.
    free(callback_message_p);

    lua_State* L = lua_getstate();  //returns main Lua state
    if (L == NULL) {
        ESP_LOGE(TAG, "Cannot retrieve main Lua state");
        return;
    }
    // put the callback reference on to the stack
    lua_rawgeti(L, LUA_REGISTRYINDEX, callback_message.callback);

    // put the measured distance on the stack
    lua_pushinteger(L, callback_message.distance);

    // invoke the Lua callback:
    int res = lua_pcall(L, 1, 0, 0);  // 1 parameter (distance), 0 return values expected, no error handler.
    if (res != 0) {
        ESP_LOGW(TAG, "Error invoking callback");
    }

    // release the reference to the callback
    luaL_unref(L, LUA_REGISTRYINDEX, callback_message.callback);
}

// txItem is a constant that defines the shape of the ping pulse
const rmt_item32_t txItem = {
    .level0 = 0,
    .duration0 = US2TICKS(20),
    .level1 = 1,
    .duration1 = US2TICKS(180),
};

// ping_task is an RTOS task that awaits for a message and then executes a ping,
// posting a message to dispatch_callback with the result.
static void ping_task(void* ctx) {
    // retrieve our userdata object out of the task parameter:
    sonar_context_t* context = (sonar_context_t*)ctx;
    sonar_message_t msg;        // to receive messages to this task
    RingbufHandle_t rb = NULL;  //buffer to receive echo messages from RMT
    esp_err_t err;
    //get RMT RX ringbuffer:

    err = rmt_get_ringbuf_handle(context->rx, &rb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot get ring buffer handle");
        abort();
    }

    err = rmt_rx_start(context->rx, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot start rmt rx");
        abort();
    }

    for (;;) {
        // start waiting for messages. xQueueReceive will block until a message is received.
        BaseType_t receive_res = xQueueReceive(context->queue, &msg, portMAX_DELAY);
        if (receive_res != pdPASS) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        // if we receive the kill message, exit the loop
        if (msg.message_type == SONAR_REMOVE) {
            break;  // exit loop to clean up and end task.
        }
        // We received the SONAR_PING message. Send the trigger pulse:
        rmt_write_items(context->tx, &txItem, 1, true);

        // Wait until sending is done.
        rmt_wait_tx_done(context->tx, portMAX_DELAY);

        // wait for the echo to come back:
        vTaskDelay(20 / portTICK_PERIOD_MS);

        // now try to read the echo pulse
        // the below loop attempts to empty the buffer, since some times
        // multiple pulses can come back, possibly due to sound taking multiple paths.
        // The below will use the last pulse received to measure the distance
        size_t rx_size = 0;
        rmt_item32_t* rxItem;
        int32_t distance = 0;
        for (;;) {
            // attempt to read a pulse:
            rxItem = (rmt_item32_t*)xRingbufferReceive(rb, &rx_size, 20 / portTICK_PERIOD_MS);
            if (!rxItem)
                break;  // nothing else received, exit loop.
            for (int i = 0; i < rx_size / sizeof(rmt_item32_t); ++i) {
                // convert the pulse duration to mm.
                distance = (int32_t)((float)TICKS2US(rxItem[i].duration0) * SPEED_OF_SOUND / 2000);
            }
            //return memory to the ring buffer.
            vRingbufferReturnItem(rb, (void*)rxItem);
        }
        // when the measured obstacle is too far away, a 0 is returned.
        // To make Lua code easier, we change this with a very big number which is out of
        // the sensor range.
        if (distance == 0) {
            distance = DISTANCE_OUT_OF_RANGE;
        }

        // prepare a message for the Lua callback event task:
        callback_message_t* callback_message_p = (callback_message_t*)malloc(sizeof(callback_message_t));
        callback_message_p->callback = msg.callback;
        callback_message_p->distance = distance;
        // send the message to the callback event task.
        // execution now continues at dispatch_callback(), but it will be run in the context of
        // NodeMCU's event queue (see task/task.c)
        task_post(0, dispatch_callback_h, (task_param_t)callback_message_p);
    }

    // mark task as deleted
    context->taskHandle = NULL;

    // actually kill this RTOS task:
    vTaskDelete(NULL);
}

// Lua: sonarobj:ping(callback)
// callback: function to invoke with the measured distance
static int sonar_ping(lua_State* L) {
    // retrieve our context out of the userdata object (first parameter, implicit)
    sonar_context_t* context = (sonar_context_t*)luaL_checkudata(L, 1, SONAR_METATABLE);

    // make sure the passed parameter is a function
    luaL_checkanyfunction(L, 2);

    // store a reference to the callback to make sure it won't be garbage collected until it is
    // time to invoke it.
    lua_ref_t callback = luaL_ref(L, LUA_REGISTRYINDEX);

    // prepare a message to send to ping_task
    sonar_message_t msg = {SONAR_PING, callback};

    // send the message to ping_task
    xQueueSend(context->queue, &msg, portMAX_DELAY);
    return 0;
}

// sonar_delete is invoked by Lua when the sonarobj is garbage collected
static int sonar_delete(lua_State* L) {
    // retrieve a reference to the userdata that will be garbage collected:
    sonar_context_t* context = (sonar_context_t*)luaL_checkudata(L, 1, SONAR_METATABLE);

    // if a task is active, send a message to it to terminate it gracefully
    if (context->taskHandle) {
        // prepare termination message:
        sonar_message_t msg = {SONAR_REMOVE, LUA_NOREF};

        // send it to ping_task:
        xQueueSend(context->queue, &msg, portMAX_DELAY);

        // wait for the task to finish:
        while (context->taskHandle)
            vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    // free up RMT channels associated with this object:
    if (context->tx > 0) {
        rmt_driver_uninstall(context->tx);
        platform_rmt_release(context->tx);
    }

    if (context->rx > 0) {
        rmt_driver_uninstall(context->rx);
        platform_rmt_release(context->rx);
    }

    // destroy the ping_task message queue
    if (context->queue) {
        vQueueDelete(context->queue);
    }

    return 0;
}

// lua: sonar.new(trig,echo)
// trig: gpio pin for the trigger signal
// echo: gpio pin for the echo signal.
// returns a sonarobj that handles the sonar connected to these pins
static int sonar_new(lua_State* L) {
    // read in the configuration pins:
    gpio_num_t trig_pin = luaL_checkinteger(L, 1);
    gpio_num_t echo_pin = luaL_checkinteger(L, 2);

    // create the userdata object to represent the sonarobj
    sonar_context_t* context = (sonar_context_t*)lua_newuserdata(L, sizeof(sonar_context_t));

    // set everything to 0 so we know what resources were initialized or not in case the this function
    // partially fails and sonar_delete is invoked prematurely as a result:
    memset(context, 0, sizeof(sonar_context_t));

    // load the sonarobj metatable definition and push it on the stack
    luaL_getmetatable(L, SONAR_METATABLE);
    lua_setmetatable(L, -2);  // apply this metatable to the userdata object

    // allocate RMT channels for this sonar
    context->tx = platform_rmt_allocate(1);
    context->rx = platform_rmt_allocate(1);

    // check whether we were able to get RMT channels
    if (context->tx < 0 || context->rx < 0) {
        luaL_error(L, "Unable to allocate RMT channel");
    }

    // initialize echo channel:
    esp_err_t err = sonar_rx_init(context->rx, echo_pin);
    if (err != ESP_OK) {
        luaL_error(L, "Unable to install rx RMT driver");
    }

    // initialize trig channel:
    err = sonar_tx_init(context->tx, trig_pin);
    if (err != ESP_OK) {
        luaL_error(L, "Unable to install tx RMT driver");
    }

    // put up a message queue so we can send messages to ping_task
    context->queue = xQueueCreate(1, sizeof(sonar_message_t));
    if (context->queue == NULL) {
        luaL_error(L, "Error creating ping queue");
    }

    // create an RTOS task to handle the ping requests:
    BaseType_t tcResult = xTaskCreate(ping_task, "sonar", 4000, context, tskIDLE_PRIORITY, &context->taskHandle);
    if (tcResult != pdPASS) {
        luaL_error(L, "Error creating ping task");
    }

    return 1;
}

// definition of the sonarobj metatable
LROT_BEGIN(sonar_metatable)
LROT_FUNCENTRY(ping, sonar_ping)
LROT_FUNCENTRY(__gc, sonar_delete)
LROT_TABENTRY(__index, sonar_metatable)
LROT_END(mqtt_metatable, NULL, 0)

// Module function map
LROT_BEGIN(sonar)
LROT_FUNCENTRY(new, sonar_new)
LROT_NUMENTRY(DISTANCE_OUT_OF_RANGE, DISTANCE_OUT_OF_RANGE)
LROT_END(sonar, NULL, 0)

int luaopen_sonar(lua_State* L) {
    luaL_rometatable(L, SONAR_METATABLE, (void*)sonar_metatable_map);  // create metatable for sonar
    dispatch_callback_h = task_get_id(dispatch_callback);
    return 0;
}

NODEMCU_MODULE(SONAR, "sonar", sonar, luaopen_sonar);
