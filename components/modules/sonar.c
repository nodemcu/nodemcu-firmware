#include <stdio.h>
#include <string.h>

#include "driver/periph_ctrl.h"
#include "driver/rmt.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lauxlib.h"
#include "lnodeaux.h"
#include "module.h"
#include "platform.h"
#include "platform_rmt.h"
#include "sdkconfig.h"
#include "soc/rmt_reg.h"
#include "task/task.h"

#define SONAR_METATABLE "sonar.mt"
#define TAG "sonar"

#define RMT_CLK_DIV 100                                  /*!< RMT counter clock divider */
#define RMT_TICK_10_US (80000000 / RMT_CLK_DIV / 100000) /*!< RMT counter value for 10 us.(Source clock is APB clock) */

#define HCSR04_MAX_TIMEOUT_US 25000 /*!< RMT receiver timeout value(us) */
#define SPEED_OF_SOUND 340          // speed of sound in m/s

#define US2TICKS(us) (us / 10 * RMT_TICK_10_US)
#define TICKS2US(ticks) (ticks * 10 / RMT_TICK_10_US)

#define DISTANCE_OUT_OF_RANGE 999999

typedef struct {
    rmt_channel_t tx;
    rmt_channel_t rx;
    lua_ref_t self;
    lua_ref_t callback;
    QueueHandle_t queue;
    TaskHandle_t taskHandle;
} sonar_context_t;

typedef enum {
    SONAR_PING,
    SONAR_REMOVE,
} sonar_message_type_t;

typedef struct {
    sonar_message_type_t message_type;
    lua_ref_t callback;
} sonar_message_t;

typedef struct {
    int32_t distance;
    lua_ref_t callback;
} callback_message_t;

static esp_err_t nec_tx_init(rmt_channel_t tx_channel, gpio_num_t gpio) {
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
    rmt_tx.rmt_mode = 0;
    esp_err_t err = rmt_config(&rmt_tx);
    if (err != ESP_OK)
        return err;
    return rmt_driver_install(rmt_tx.channel, 0, 0);
}

static esp_err_t nec_rx_init(rmt_channel_t rx_channel, gpio_num_t gpio) {
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

task_handle_t dispatchCallbackHandle;
static void dispatchCallback(task_param_t param, task_prio_t prio) {
    callback_message_t* callback_message_p = (callback_message_t*)param;
    callback_message_t callback_message = *callback_message_p;
    free(callback_message_p);

    lua_State* L = lua_getstate();  //returns main Lua state
    if (L == NULL) {
        ESP_LOGE(TAG, "Cannot retrieve main Lua state");
        return;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, callback_message.callback);
    lua_pushinteger(L, callback_message.distance);
    int res = lua_pcall(L, 1, 0, 0);
    if (res != 0) {
        ESP_LOGW(TAG, "Error invoking callback");
    }
    luaL_unref(L, LUA_REGISTRYINDEX, callback_message.callback);
}

static void runPing(void* ctx) {
    ESP_LOGW(TAG, "runPing start");
    sonar_context_t* context = (sonar_context_t*)ctx;
    ESP_LOGW(TAG, "runPing ctx=%x", (uint32_t)context);
    sonar_message_t msg;

    RingbufHandle_t rb = NULL;

    //get RMT RX ringbuffer
    ESP_LOGW(TAG, "get rx ringbuffer");
    rmt_get_ringbuf_handle(context->rx, &rb);
    rmt_rx_start(context->rx, 1);

    rmt_item32_t txItem;

    txItem.level0 = 0;
    txItem.duration0 = US2TICKS(20);
    txItem.level1 = 1;
    txItem.duration1 = US2TICKS(180);

    for (;;) {
        ESP_LOGW(TAG, "begin wait");
        BaseType_t receive_res = xQueueReceive(context->queue, &msg, portMAX_DELAY);
        if (receive_res != pdPASS) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGW(TAG, "got message=%d", msg.message_type);

        if (msg.message_type == SONAR_REMOVE) {
            break;  // exit loop to clean up and end task.
        }

        rmt_write_items(context->tx, &txItem, 1, true);
        // Wait until sending is done.
        rmt_wait_tx_done(context->tx, portMAX_DELAY);
        vTaskDelay(20 / portTICK_PERIOD_MS);
        size_t rx_size = 0;
        //try to receive data from ringbuffer.
        //RMT driver will push all the data it receives to its ringbuffer.
        //We just need to parse the value and return the spaces of ringbuffer.
        ESP_LOGW(TAG, "Ringbuffer receive");
        rmt_item32_t* rxItem;
        int32_t distance = 0;
        for (;;) {
            rxItem = (rmt_item32_t*)xRingbufferReceive(rb, &rx_size, 20 / portTICK_PERIOD_MS);
            if (!rxItem)
                break;
            for (int i = 0; i < rx_size / sizeof(rmt_item32_t); ++i) {
                distance = (int32_t)((float)TICKS2US(rxItem[i].duration0) * SPEED_OF_SOUND / 2000);
                ESP_LOGD(TAG, "RMT RCV; %d:%d | %d:%d : %.1fcm",
                         rxItem[i].level0, TICKS2US(rxItem[i].duration0),
                         rxItem[i].level1, TICKS2US(rxItem[i].duration1),
                         distance / 10);
            }
            //after parsing the data, return spaces to ringbuffer.
            vRingbufferReturnItem(rb, (void*)rxItem);
        }
        if (distance == 0) {
            distance = DISTANCE_OUT_OF_RANGE;
        }
        callback_message_t* callback_message_p = (callback_message_t*)malloc(sizeof(callback_message_t));
        callback_message_p->callback = msg.callback;
        callback_message_p->distance = distance;
        task_post(0, dispatchCallbackHandle, (task_param_t)callback_message_p);
    }

    ESP_LOGW(TAG, "runPing end");
    context->taskHandle = NULL;
    vTaskDelete(NULL);
}

// Lua: sonar:ping(callback)
static int sonar_ping(lua_State* L) {
    sonar_context_t* context = (sonar_context_t*)luaL_checkudata(L, 1, SONAR_METATABLE);
    luaL_checkanyfunction(L, 2);
    lua_ref_t callback = luaL_ref(L, LUA_REGISTRYINDEX);
    sonar_message_t msg = {SONAR_PING, callback};
    xQueueSend(context->queue, &msg, portMAX_DELAY);
    return 0;
}

static int sonar_delete(lua_State* L) {
    sonar_context_t* context = (sonar_context_t*)luaL_checkudata(L, 1, SONAR_METATABLE);
    if (context->taskHandle) {
        sonar_message_t msg = {SONAR_REMOVE, LUA_NOREF};
        xQueueSend(context->queue, &msg, portMAX_DELAY);
        while (context->taskHandle)
            vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    if (context->tx > 0) {
        rmt_driver_uninstall(context->tx);
        platform_rmt_release(context->tx);
    }

    if (context->rx > 0) {
        rmt_driver_uninstall(context->rx);
        platform_rmt_release(context->rx);
    }

    if (context->queue) {
        vQueueDelete(context->queue);
    }

    return 0;
}

static int sonar_setup(lua_State* L) {
    gpio_num_t echo_pin = luaL_checkinteger(L, 1);
    gpio_num_t trig_pin = luaL_checkinteger(L, 2);

    sonar_context_t* context = (sonar_context_t*)lua_newuserdata(L, sizeof(sonar_context_t));
    memset(context, 0, sizeof(sonar_context_t));
    context->callback = context->self = LUA_NOREF;
    luaL_getmetatable(L, SONAR_METATABLE);
    lua_setmetatable(L, -2);

    context->tx = platform_rmt_allocate(1);
    context->rx = platform_rmt_allocate(1);

    if (context->tx < 0 || context->rx < 0) {
        luaL_error(L, "Unable to allocate RMT channel");
    }

    esp_err_t err = nec_rx_init(context->rx, echo_pin);
    if (err != ESP_OK) {
        luaL_error(L, "Unable to install rx RMT driver");
    }
    err = nec_tx_init(context->tx, trig_pin);
    if (err != ESP_OK) {
        luaL_error(L, "Unable to install tx RMT driver");
    }

    context->queue = xQueueCreate(1, sizeof(sonar_message_t));
    if (context->queue == NULL) {
        luaL_error(L, "Error creating ping queue");
    }

    BaseType_t tcResult = xTaskCreate(runPing, "sonar", 4000, context, tskIDLE_PRIORITY, &context->taskHandle);
    if (tcResult != pdPASS) {
        luaL_error(L, "Error creating ping task");
    }
    ESP_LOGW(TAG, "task created ctx=%x", (uint32_t)context);

    return 1;
}

// map client methods to functions:
LROT_BEGIN(sonar_metatable)
LROT_FUNCENTRY(ping, sonar_ping)
LROT_FUNCENTRY(__gc, sonar_delete)
LROT_TABENTRY(__index, sonar_metatable)
LROT_END(mqtt_metatable, NULL, 0)

// Module function map
LROT_BEGIN(sonar)
LROT_FUNCENTRY(setup, sonar_setup)
LROT_END(sonar, NULL, 0)

int luaopen_sonar(lua_State* L) {
    luaL_rometatable(L, SONAR_METATABLE, (void*)sonar_metatable_map);  // create metatable for sonar
    dispatchCallbackHandle = task_get_id(dispatchCallback);
    return 0;
}

NODEMCU_MODULE(SONAR, "sonar", sonar, luaopen_sonar);
