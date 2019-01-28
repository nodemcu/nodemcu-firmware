// Module for interfacing with an MQTT broker
#define CONFIG_NODE_DEBUG 1
#include "lauxlib.h"
#include "lextra.h"
#include "lmem.h"
#include "module.h"
#include "platform.h"
#include "task/task.h"

#include <string.h>
#include "mqtt_client.h"

#define MQTT_MAX_HOST_LEN 64
#define MQTT_MAX_CLIENT_LEN 32
#define MQTT_MAX_USERNAME_LEN 32
#define MQTT_MAX_PASSWORD_LEN 65
#define MQTT_MAX_LWT_TOPIC 32
#define MQTT_MAX_LWT_MSG 128
#define MQTT_METATABLE "mqtt.mt"

// mqtt_context struct contains information to wrap a esp_mqtt client in lua
struct mqtt_context {
    esp_mqtt_client_handle_t client;  // handle to mqtt client
    char* client_id;                  // mqtt client ID
    char* username;                   // mqtt username
    char* password;                   // mqtt password
    char* lwt_topic;                  // mqtt last will/testament topic
    char* lwt_msg;                    // mqtt last will message
    int lwt_qos;                      // mqtt LWT qos level
    int lwt_retain;                   // mqtt LWT retain flag
    int keepalive;                    // keepalive ping period in seconds
    int disable_clean_session;        // Whether to not clean the session on reconnect
    union {
        struct {
            lua_ref_t on_connect_cb;  // maps to "connect" event
            lua_ref_t on_message_cb;  // maps to "message" event
            lua_ref_t on_offline_cb;  // maps to "offline" event
            lua_ref_t connected_ok_cb;
            lua_ref_t connected_nok_cb;
            lua_ref_t published_ok_cb;
            lua_ref_t subscribed_ok_cb;
            lua_ref_t unsubscribed_ok_cb;
            lua_ref_t self;
        };
        lua_ref_t event_cb[9];
    };
    struct mqtt_context** pcontext;
};

// eventnames contains a list of the events that can be set in lua
// with client:on(eventName, function)
// The order is important, as they map directly to callbacks
// in the union/struct above
const char* const eventnames[] = {"connect", "message", "offline", NULL};

typedef struct mqtt_context mqtt_context_t;

// rtos task handlers for the different events
task_handle_t connected_task_id = 0;
task_handle_t disconnected_task_id = 0;
task_handle_t publish_task_id = 0;
task_handle_t subscribe_task_id = 0;
task_handle_t unsubscribe_task_id = 0;
task_handle_t data_task_id = 0;

// event_clone makes a copy of the mqtt event received so we can pass it on
// and the mqtt library can discard it.
static esp_mqtt_event_handle_t event_clone(esp_mqtt_event_handle_t ev) {
    // allocate memory for the copy
    esp_mqtt_event_handle_t ev1 = (esp_mqtt_event_handle_t)malloc(sizeof(esp_mqtt_event_t));
    NODE_DBG("event_clone():malloc: event %p, msg %d\n", ev, ev->msg_id);

    // make a shallow copy:
    *ev1 = *ev;

    // if the event carries data, make also a copy of it.
    if (ev->data != NULL) {
        if (ev->data_len > 0) {
            ev1->data = malloc(ev->data_len + 1);  // null-terminate the data, useful for debugging
            memcpy(ev1->data, ev->data, ev->data_len);
            ev1->data[ev1->data_len] = '\0';
            NODE_DBG("event_clone():malloc: event %p, msg %d, data %p, num %d\n", ev1, ev1->msg_id, ev1->data, ev1->data_len);
        } else {
            ev1->data = NULL;
        }
    }

    // if the event carries a topic, make also a copy of it.
    if (ev->topic != NULL) {
        if (ev->topic_len > 0) {
            ev1->topic = malloc(ev->topic_len + 1);  // null-terminate the data, useful for debugging
            memcpy(ev1->topic, ev->topic, ev->topic_len);
            ev1->topic[ev1->topic_len] = '\0';
            NODE_DBG("event_clone():malloc: event %p, msg %d, topic %p, num %d\n", ev1, ev1->msg_id, ev1->topic, ev1->topic_len);
        } else {
            ev1->topic = NULL;
        }
    }
    return ev1;
}

// event_free deallocates all the memory associated with a cloned event
static void event_free(esp_mqtt_event_handle_t ev) {
    if (ev->data != NULL) {
        NODE_DBG("event_free():free: event %p, msg %d, data %p\n", ev, ev->msg_id, ev->data);
        free(ev->data);
    }
    if (ev->topic != NULL) {
        NODE_DBG("event_free():free: event %p, msg %d, topic %p\n", ev, ev->msg_id, ev->topic);
        free(ev->topic);
    }
    free(ev);
}

// mqtt_event_handler receives all events from the esp mqtt library and converts them
// to task messages
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
    mqtt_context_t** pcontext = (mqtt_context_t**)event->user_context;
    NODE_DBG("event_handler: mqtt_context*: %p\n", *pcontext);

    // Check if this event is about an object that has been already garbage collected:
    if (*pcontext == NULL) {
        NODE_DBG("caught stray event: %d\n", event->event_id);  // this can happen if the userdata object is dereferenced while attempting to connect
        return ESP_OK;
    }

    // Dispatch the event to the appropriate task:
    NODE_DBG("mqtt_event_handler: %d\n", event->event_id);
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            task_post_medium(connected_task_id, (task_param_t)event_clone(event));
            break;

        case MQTT_EVENT_DISCONNECTED:
            task_post_medium(disconnected_task_id, (task_param_t)event_clone(event));
            break;

        case MQTT_EVENT_SUBSCRIBED:
            task_post_medium(subscribe_task_id, (task_param_t)event_clone(event));
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            task_post_medium(unsubscribe_task_id, (task_param_t)event_clone(event));
            break;

        case MQTT_EVENT_PUBLISHED:
            task_post_medium(publish_task_id, (task_param_t)event_clone(event));
            break;

        case MQTT_EVENT_DATA:
            task_post_medium(data_task_id, (task_param_t)event_clone(event));
            break;

        case MQTT_EVENT_ERROR:
            break;
    }
    return ESP_OK;
}

// task_connected is run when the mqtt client connected
static void task_connected(task_param_t param, task_prio_t prio) {
    lua_State* L = lua_getstate();  //returns main Lua state
    if (L == NULL)
        return;

    // extract the event data out of the task param
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)param;

    // recover the mqtt context from the event user_context field:
    mqtt_context_t* mqtt_context = *(mqtt_context_t**)event->user_context;
    NODE_DBG("CB:connect: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));

    event_free(event);  // free the event copy memory

    if (mqtt_context->self <= 0) {  // if this reference is unset something weird is happening
        NODE_DBG("CB:connect: Received event on a collected object\n");
        return;
    }

    int top = lua_gettop(L);  // save the stack status to restore it later
    lua_checkstack(L, 3);     // make sure there are at least 3 slots available

    // pin our object by putting a reference on the stack,
    // so it can't be garbage collected during user callback execution.
    luaL_push_weak_ref(L, mqtt_context->self);

    // if the user set a one-shot connected callback, execute it:
    if (mqtt_context->connected_ok_cb > 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->connected_ok_cb);  // push the callback function reference to the stack
        luaL_push_weak_ref(L, mqtt_context->self);                         // push a reference to the client (first parameter)

        NODE_DBG("CB:connect: calling registered one-shot connect callback\n");
        int res = lua_pcall(L, 1, 0, 0);  //call the connect callback: function(client)
        if (res != 0)
            NODE_DBG("CB:connect: Error when calling one-shot connect callback - (%d) %s\n", res, luaL_checkstring(L, -1));

        //after connecting ok, we clear _both_ the one-shot callbacks:
        unset_ref(L, &mqtt_context->connected_ok_cb);
        unset_ref(L, &mqtt_context->connected_nok_cb);
    }
    lua_settop(L, top);

    // now we check for the standard connect callback registered with 'mqtt:on()'
    if (mqtt_context->on_connect_cb > 0) {
        NODE_DBG("CB:connect: calling registered standard connect callback\n");
        lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->on_connect_cb);  // push the callback function reference to the stack
        luaL_push_weak_ref(L, mqtt_context->self);                       // push a reference to the client (first parameter)
        int res = lua_pcall(L, 1, 0, 0);                                 //call the connect callback: function(client)
        if (res != 0)
            NODE_DBG("CB:connect: Error when calling connect callback - (%d) %s\n", res, luaL_checkstring(L, -1));
    }

    lua_settop(L, top);  // leave the stack as it was
}

// task_disconnected is run after a connection to the MQTT broker breaks.
static void task_disconnected(task_param_t param, task_prio_t prio) {
    lua_State* L = lua_getstate();  //returns main Lua state
    if (L == NULL)
        return;

    // extract the event data out of the task param
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)param;

    // recover the mqtt context from the event user_context field:
    mqtt_context_t* mqtt_context = *(mqtt_context_t**)event->user_context;
    NODE_DBG("CB:disconnect: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));

    event_free(event);  // free the event copy memory

    if (mqtt_context->client == NULL) {
        NODE_DBG("MQTT Client was NULL on a disconnect event\n");
    }

    // destroy the wrapped mqtt_client object
    esp_mqtt_client_destroy(mqtt_context->client);
    mqtt_context->client = NULL;

    if (mqtt_context->self <= 0) {  // if this reference is unset something weird is happening
        NODE_DBG("CB:disconnect: Received event on a collected object\n");
        return;
    }

    int top = lua_gettop(L);  // save the stack status to restore it later
    lua_checkstack(L, 4);     // make sure there are at least 4 slots available

    // pin our object by putting a reference on the stack,
    // so it can't be garbage collected during user callback execution.
    luaL_push_weak_ref(L, mqtt_context->self);

    // if the user set a one-shot connect error callback, execute it:
    if (mqtt_context->connected_nok_cb > 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->connected_nok_cb);  // push the callback function reference to the stack
        luaL_push_weak_ref(L, mqtt_context->self);                          // push a reference to the client (first parameter)
        lua_pushinteger(L, -6);                                             // esp sdk mqtt lib does not provide reason codes. Push "-6" to be backward compatible with ESP8266 API

        NODE_DBG("CB:disconnect: calling registered one-shot disconnect callback\n");
        int res = lua_pcall(L, 2, 0, 0);  //call the disconnect callback with 2 parameters: function(client, reason)
        if (res != 0)
            NODE_DBG("CB:disconnect: Error when calling one-shot disconnect callback - (%d) %s\n", res, luaL_checkstring(L, -1));

        //after connecting ok, we clear _both_ the one-shot callbacks
        unset_ref(L, &mqtt_context->connected_ok_cb);
        unset_ref(L, &mqtt_context->connected_nok_cb);
    }

    // now we check for the standard offline callback registered with 'mqtt:on()'
    if (mqtt_context->on_offline_cb > 0) {
        NODE_DBG("CB:disconnect: calling registered standard on_offline_cb callback\n");
        lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->on_offline_cb);  // push the callback function reference to the stack
        luaL_push_weak_ref(L, mqtt_context->self);                       // push a reference to the client (first parameter)
        int res = lua_pcall(L, 1, 0, 0);                                 //call the offline callback: function(client)
        if (res != 0)
            NODE_DBG("CB:disconnect: Error when calling offline callback - (%d) %s\n", res, luaL_checkstring(L, -1));
    }

    lua_settop(L, top);  //leave stack as we found it
}

// task_subscribe is called when the last subscribe call is successful
static void task_subscribe(task_param_t param, task_prio_t prio) {
    lua_State* L = lua_getstate();  //returns main Lua state
    if (L == NULL)
        return;

    // extract the event data out of the task param
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)param;

    // recover the mqtt context from the event user_context field:
    mqtt_context_t* mqtt_context = *(mqtt_context_t**)event->user_context;

    NODE_DBG("CB:subscribe: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));
    event_free(event);  // free the event copy memory

    if (mqtt_context->self <= 0) {  // if this reference is unset something weird is happening
        NODE_DBG("CB:subscribe: Received event on a collected object\n");
        return;
    }

    int top = lua_gettop(L);  // save the stack status to restore it later
    lua_checkstack(L, 3);     // make sure there are at least 3 slots available

    // pin our object by putting a reference on the stack,
    // so it can't be garbage collected during user callback execution.
    luaL_push_weak_ref(L, mqtt_context->self);

    // if there is a subscribe one-shot callback, execute it:
    if (mqtt_context->subscribed_ok_cb > 0) {
        NODE_DBG("CB:subscribe: calling registered one-shot subscribe callback\n");
        lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->subscribed_ok_cb);  // push the function reference on the stack
        luaL_push_weak_ref(L, mqtt_context->self);                          // push the client object on the stack
        int res = lua_pcall(L, 1, 0, 0);                                    //call the connect callback with one parameter: function(client)
        if (res != 0)
            NODE_DBG("CB:subscribe: Error when calling one-shot subscribe callback - (%d) %s\n", res, luaL_checkstring(L, -1));

        unset_ref(L, &mqtt_context->subscribed_ok_cb);  // forget the callback since it is one-shot
    }

    lua_settop(L, top);  //leave stack as it was
}

//task_publish is called when a publish operation completes
static void task_publish(task_param_t param, task_prio_t prio) {
    NODE_DBG("CB:publish: successfully transferred control back to main task\n");

    lua_State* L = lua_getstate();  //returns main Lua state
    if (L == NULL)
        return;

    // extract the event data out of the task param
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)param;

    // recover the mqtt context from the event user_context field:
    mqtt_context_t* mqtt_context = *(mqtt_context_t**)event->user_context;

    NODE_DBG("CB:publish: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));
    event_free(event);  // free the event copy memory

    if (mqtt_context->self <= 0) {  // if this reference is unset something weird is happening
        NODE_DBG("CB:publish: Received event on a collected object\n");
        return;
    }

    int top = lua_gettop(L);  // save the stack status to restore it later
    lua_checkstack(L, 3);     // make sure there are at least 3 slots available

    // pin our object by putting a reference on the stack,
    // so it can't be garbage collected during user callback execution.
    luaL_push_weak_ref(L, mqtt_context->self);

    // if there is a one-shot callback set, execute it:
    if (mqtt_context->published_ok_cb > 0) {
        NODE_DBG("CB:publish: calling registered one-shot publish callback\n");
        lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->published_ok_cb);  // push the callback function reference to the stack
        luaL_push_weak_ref(L, mqtt_context->self);                         // push the client reference to the stack
        int res = lua_pcall(L, 1, 0, 0);                                   //call the connect callback with 1 parameter: function(client)
        if (res != 0)
            NODE_DBG("CB:publish: Error when calling one-shot publish callback - (%d) %s\n", res, luaL_checkstring(L, -1));

        unset_ref(L, &mqtt_context->published_ok_cb);  // forget this callback since it is one-shot
    }
    lua_settop(L, top);
}

// task_unsubscribe is called when a subscription is successful
static void task_unsubscribe(task_param_t param, task_prio_t prio) {
    lua_State* L = lua_getstate();  //returns main Lua state
    if (L == NULL)
        return;

    // extract the event data out of the task param
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)param;

    // recover the mqtt context from the event user_context field:
    mqtt_context_t* mqtt_context = *(mqtt_context_t**)event->user_context;

    NODE_DBG("CB:unsubscribe: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));
    event_free(event);  // free the event copy memory

    if (mqtt_context->self <= 0) {  // if this reference is unset something weird is happening
        NODE_DBG("CB:unsubscribe: Received event on a collected object\n");
        return;
    }

    int top = lua_gettop(L);  // save the stack status to restore it later
    lua_checkstack(L, 3);     // make sure there are at least 3 slots available

    // pin our object by putting a reference on the stack,
    // so it can't be garbage collected during user callback execution.
    luaL_push_weak_ref(L, mqtt_context->self);

    // if there is a one-shot callback set, execute it:
    if (mqtt_context->unsubscribed_ok_cb > 0) {
        NODE_DBG("CB:unsubscribe: calling registered one-shot unsubscribe callback\n");
        lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->unsubscribed_ok_cb);  // push callback function reference on the stack
        luaL_push_weak_ref(L, mqtt_context->self);                            // push a reference to the client
        int res = lua_pcall(L, 1, 0, 0);                                      //call the connect callback with one parameter: function(client)
        if (res != 0)
            NODE_DBG("CB:unsubscribe: Error when calling one-shot unsubscribe callback - (%d) %s\n", res, luaL_checkstring(L, -1));

        unset_ref(L, &mqtt_context->unsubscribed_ok_cb);  // forget callback as it is one-shot
    }
    lua_settop(L, top);
}

//task_data_received is called when data is received on a subscribed topic
static void task_data_received(task_param_t param, task_prio_t prio) {
    lua_State* L = lua_getstate();  //returns main Lua state
    if (L == NULL)
        return;

    // extract the event data out of the task param
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)param;

    // recover the mqtt context from the event user_context field:
    mqtt_context_t* mqtt_context = *(mqtt_context_t**)event->user_context;

    NODE_DBG("CB:data: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));
    event_free(event);  // free the event copy memory

    if (mqtt_context->self <= 0) {  // if this reference is unset something weird is happening
        NODE_DBG("CB:data: Received event on a collected object\n");
        return;
    }

    int top = lua_gettop(L);  // save the stack status to restore it later
    lua_checkstack(L, 5);     // make sure there are at least 3 slots available

    // pin our object by putting a reference on the stack,
    // so it can't be garbage collected during user callback execution.
    luaL_push_weak_ref(L, mqtt_context->self);

    if (mqtt_context->on_message_cb > 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->on_message_cb);
        int numArg = 2;
        luaL_push_weak_ref(L, mqtt_context->self);
        lua_pushlstring(L, event->topic, event->topic_len);
        if (event->data != NULL) {
            lua_pushlstring(L, event->data, event->data_len);
            numArg++;
        }
        int res = lua_pcall(L, numArg, 0, 0);  //call the messagecallback
        if (res != 0)
            NODE_DBG("CB:data: Error when calling message callback - (%d) %s\n", res, luaL_checkstring(L, -1));
    }
    lua_settop(L, top);
    event_free(event);
}

// Lua: on()
// mqtt_on allows to set the callback associated to mqtt events
static int mqtt_on(lua_State* L) {
    if (!lua_isfunction(L, 3))  //check whether we are passed a callback function
        return 0;

    int event = luaL_checkoption(L, 2, "message", eventnames);  // map passed event name to an index in the eventnames array

    mqtt_context_t* mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, MQTT_METATABLE);  //retrieve the mqtt_context

    set_ref(L, 3, &mqtt_context->event_cb[event]);  // set the callback reference

    return 0;
}

// Lua: mqtt:connect(host[, port[, secure[, autoreconnect]]][, function(client)[, function(client, reason)]])
// mqtt_connect starts a connection with the mqtt broker
static int mqtt_connect(lua_State* L) {
    mqtt_context_t* mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, MQTT_METATABLE);  //retrieve the mqtt context

    if (mqtt_context->client) {  // destroy existing client. This disconnects an existing connection using this object
        esp_mqtt_client_destroy(mqtt_context->client);
        mqtt_context->client = NULL;
    }

    // initialize a mqtt config structure set to zero
    esp_mqtt_client_config_t config;
    memset(&config, 0, sizeof(esp_mqtt_client_config_t));

    // process function parameters populating the mqtt config structure
    config.host = luaL_checkstring(L, 2);

    // set defaults:
    int secure = 0;
    int reconnect = 0;
    int port = 1883;
    int n = 3;

    if (lua_isnumber(L, n)) {
        port = luaL_checknumber(L, n);
        n++;
    }

    if (lua_isnumber(L, n)) {
        secure = !!luaL_checkinteger(L, n);
        n++;
    }

    if (lua_isnumber(L, n)) {
        reconnect = !!luaL_checkinteger(L, n);
        n++;
    }

    if (lua_isfunction(L, n)) {
        set_ref(L, n, &mqtt_context->connected_ok_cb);
        n++;
    }

    if (lua_isfunction(L, n)) {
        set_ref(L, n, &mqtt_context->connected_nok_cb);
        n++;
    }

    NODE_DBG("connect: mqtt_context*: %p\n", mqtt_context);

    config.user_context = mqtt_context->pcontext;  // store a pointer to our context in the mqtt client user context field
                                                   // this will be useful to identify to which instance events belong to
    config.event_handle = mqtt_event_handler;      // set the function that will be called by the mqtt client everytime something
                                                   // happens

    config.client_id = mqtt_context->client_id;
    config.lwt_msg = mqtt_context->lwt_msg;
    config.lwt_topic = mqtt_context->lwt_topic;
    config.username = mqtt_context->username;
    config.password = mqtt_context->password;
    config.keepalive = mqtt_context->keepalive;
    config.disable_clean_session = mqtt_context->disable_clean_session;
    config.port = port;
    config.disable_auto_reconnect = (reconnect == 0);
    config.transport = secure ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;

    // create a mqtt client instance
    mqtt_context->client = esp_mqtt_client_init(&config);
    if (mqtt_context->client == NULL) {
        luaL_error(L, "MQTT library failed to start");
        return 0;
    }

    // actually start the mqtt client and connect
    esp_err_t err = esp_mqtt_client_start(mqtt_context->client);
    if (err != ESP_OK) {
        luaL_error(L, "Error starting mqtt client");
    }

    lua_pushboolean(L, true);  // return true (ok)
    return 1;
}

// Lua: mqtt:close()
// mqtt_close terminates the current connection
static int mqtt_close(lua_State* L) {
    mqtt_context_t* mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, MQTT_METATABLE);

    if (mqtt_context->client == NULL)
        return 0;

    NODE_DBG("Closing MQTT client %p\n", mqtt_context->client);

    esp_mqtt_client_destroy(mqtt_context->client);
    mqtt_context->client = NULL;

    return 0;
}

// Lua: mqtt:lwt(topic, message[, qos[, retain]])
// mqtt_lwt sets last will / testament topic and message
// must be called before connecting
static int mqtt_lwt(lua_State* L) {
    mqtt_context_t* mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, MQTT_METATABLE);

    // free previous topic and messasge, if any.
    free_string(L, mqtt_context->lwt_topic);
    free_string(L, mqtt_context->lwt_msg);

    // save a copy of topic and message to pass to the client
    // when connecting
    mqtt_context->lwt_topic = alloc_string(L, 2, MQTT_MAX_LWT_TOPIC);
    mqtt_context->lwt_msg = alloc_string(L, 3, MQTT_MAX_LWT_MSG);

    //process optional parameters
    int n = 4;
    if (lua_isnumber(L, n)) {
        mqtt_context->lwt_qos = (int)lua_tonumber(L, n);
        n++;
    }

    if (lua_isnumber(L, n)) {
        mqtt_context->lwt_retain = (int)lua_tonumber(L, n);
        n++;
    }

    NODE_DBG("Set LWT topic '%s', qos %d, retain %d\n",
             mqtt_context->lwt_topic, mqtt_context->lwt_qos, mqtt_context->lwt_retain);
    return 0;
}

//Lua: mqtt:publish(topic, payload, qos, retain[, function(client)])
// returns true on success, false otherwise
// mqtt_publish publishes a message on the given topic
static int mqtt_publish(lua_State* L) {
    mqtt_context_t* mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, MQTT_METATABLE);
    esp_mqtt_client_handle_t client = mqtt_context->client;

    if (client == NULL) {
        lua_pushboolean(L, false);  // return false (error)
        return 1;
    }

    const char* topic = luaL_checkstring(L, 2);
    size_t data_size;
    const char* data = luaL_checklstring(L, 3, &data_size);
    int qos = luaL_checkint(L, 4);
    int retain = luaL_checkint(L, 5);

    if (lua_isfunction(L, 6)) {  // set one-shot on publish callback
        set_ref(L, 6, &mqtt_context->published_ok_cb);
    }

    NODE_DBG("MQTT publish client %p, topic %s, %d bytes\n", client, topic, data_size);
    int msg_id = esp_mqtt_client_publish(client, topic, data, data_size, qos, retain);

    lua_pushboolean(L, msg_id >= 0);  // if msg_id < 0 there was an error.
    return 1;
}

// Lua: mqtt:subscribe(topic, qos[, function(client)]) OR mqtt:subscribe(table[, function(client)])
// returns true on success, false otherwise
// mqtt_subscribe subscribes to the given topic
static int mqtt_subscribe(lua_State* L) {
    mqtt_context_t* mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, MQTT_METATABLE);
    esp_mqtt_client_handle_t client = mqtt_context->client;

    if (client == NULL) {
        lua_pushboolean(L, false);  // return false (error)
        return 1;
    }

    const char* topic = luaL_checkstring(L, 2);
    int qos = luaL_checkint(L, 3);

    if (lua_isfunction(L, 4))  // if a callback is provided, set it.
        set_ref(L, 4, &mqtt_context->subscribed_ok_cb);

    NODE_DBG("MQTT subscribe client %p, topic %s\n", client, topic);

    esp_err_t err = esp_mqtt_client_subscribe(client, topic, qos);
    lua_pushboolean(L, err == ESP_OK);

    return 1;  // one value returned, true on success, false on error.
}

// Lua: mqtt:unsubscribe(topic[, function(client)])
// TODO: accept also mqtt:unsubscribe(table[, function(client)])
// returns true on success, false otherwise
// mqtt_unsubscribe unsubscribes from the given topic
static int mqtt_unsubscribe(lua_State* L) {
    mqtt_context_t* mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, MQTT_METATABLE);
    esp_mqtt_client_handle_t client = mqtt_context->client;

    if (client == NULL) {
        lua_pushboolean(L, false);  // return false (error)
        return 1;
    }

    const char* topic = luaL_checkstring(L, 2);
    if (lua_isfunction(L, 3))
        set_ref(L, 3, &mqtt_context->unsubscribed_ok_cb);

    NODE_DBG("MQTT unsubscribe client %p, topic %s\n", client, topic);

    esp_err_t err = esp_mqtt_client_unsubscribe(client, topic);
    lua_pushboolean(L, err == ESP_OK);

    return 1;  // return 1 value: true OK, false error.
}

// mqtt_deleted is called on garbage collection
static int mqtt_delete(lua_State* L) {
    mqtt_context_t* mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, MQTT_METATABLE);

    // if there is a client active, shut it down.
    if (mqtt_context->client != NULL) {
        NODE_DBG("stopping MQTT client %p; *mqtt_context->pcontext=%p\n", mqtt_context->client, *(mqtt_context->pcontext));
        *(mqtt_context->pcontext) = NULL;  // unlink mqtt_client's user_context to this object
                                           // destroy the client. This is a blocking call. If a connection request was ongoing this will block and
                                           // a disconnect callback could be fired.
        esp_mqtt_client_destroy(mqtt_context->client);
    }

    // free the memory used to link mqtt client user context to our lua userdata object
    luaM_freemem(L, mqtt_context->pcontext, sizeof(mqtt_context_t*));

    // forget all callbacks
    for (int i = 0; i < sizeof(mqtt_context->event_cb) / sizeof(lua_ref_t); i++) {
        unset_ref(L, &mqtt_context->event_cb[i]);
    }

    // free all dynamic strings
    free_string(L, mqtt_context->client_id);
    free_string(L, mqtt_context->username);
    free_string(L, mqtt_context->password);
    free_string(L, mqtt_context->lwt_msg);
    free_string(L, mqtt_context->lwt_topic);

    NODE_DBG("MQTT client garbage collected\n");
    return 0;
}

// Lua: mqtt.Client(clientid, keepalive[, username, password, cleansession])
// mqtt_new creates a new instance of our mqtt userdata lua object
static int mqtt_new(lua_State* L) {
    //create a new lua userdata object and initialize to 0.
    mqtt_context_t* mqtt_context = (mqtt_context_t*)lua_newuserdata(L, sizeof(mqtt_context_t));
    memset(mqtt_context, 0, sizeof(mqtt_context_t));

    // initialize all callbacks to LUA_NOREF, indicating they're unset.
    for (int i = 0; i < sizeof(mqtt_context->event_cb) / sizeof(lua_ref_t); i++) {
        mqtt_context->event_cb[i] = LUA_NOREF;
    }

    // keep a weak reference to our userdata object so we can pass it as a parameter to user callbacks
    lua_pushvalue(L, -1);
    mqtt_context->self = luaL_weak_ref(L);

    // allocate a pointer that will be used to link the mqtt client user context to this lua userdata object
    mqtt_context->pcontext = luaM_malloc(L, sizeof(mqtt_context_t*));
    *(mqtt_context->pcontext) = mqtt_context;  //set it to point to this lua userdata object

    // store the parameters passed:
    mqtt_context->client_id = alloc_string(L, 1, MQTT_MAX_CLIENT_LEN);
    NODE_DBG("MQTT client id %s\n", mqtt_context->client_id);

    mqtt_context->keepalive = luaL_checkinteger(L, 2);

    int n = 2;
    if (lua_isstring(L, 3)) {
        mqtt_context->username = alloc_string(L, 3, MQTT_MAX_USERNAME_LEN);
        n++;
    }

    if (lua_isstring(L, 4)) {
        mqtt_context->password = alloc_string(L, 4, MQTT_MAX_PASSWORD_LEN);
        n++;
    }

    if (lua_isnumber(L, 5)) {
        mqtt_context->disable_clean_session = (luaL_checknumber(L, 5) == 0);
        n++;
    }

    luaL_getmetatable(L, MQTT_METATABLE);
    lua_setmetatable(L, -2);

    if (connected_task_id == 0) {  // if this is the first time, create rtos tasks for every event type
        connected_task_id = task_get_id(task_connected);
        disconnected_task_id = task_get_id(task_disconnected);
        publish_task_id = task_get_id(task_publish);
        subscribe_task_id = task_get_id(task_subscribe);
        unsubscribe_task_id = task_get_id(task_unsubscribe);
        data_task_id = task_get_id(task_data_received);
        NODE_DBG("conn %d, off %d, pub %d, sub %d, data %d\n", connected_task_id, disconnected_task_id, publish_task_id, subscribe_task_id, data_task_id);
    }

    return 1;  //one object returned, the mqtt context wrapped in a lua userdata object
}

// map client methods to functions:
static const LUA_REG_TYPE mqtt_metatable_map[] =
    {
        {LSTRKEY("connect"), LFUNCVAL(mqtt_connect)},
        {LSTRKEY("close"), LFUNCVAL(mqtt_close)},
        {LSTRKEY("lwt"), LFUNCVAL(mqtt_lwt)},
        {LSTRKEY("publish"), LFUNCVAL(mqtt_publish)},
        {LSTRKEY("subscribe"), LFUNCVAL(mqtt_subscribe)},
        {LSTRKEY("unsubscribe"), LFUNCVAL(mqtt_unsubscribe)},
        {LSTRKEY("on"), LFUNCVAL(mqtt_on)},
        {LSTRKEY("__gc"), LFUNCVAL(mqtt_delete)},
        {LSTRKEY("__index"), LROVAL(mqtt_metatable_map)},
        {LNILKEY, LNILVAL}};

// Module function map
static const LUA_REG_TYPE mqtt_map[] = {
    {LSTRKEY("Client"), LFUNCVAL(mqtt_new)},
    {LNILKEY, LNILVAL}};

int luaopen_mqtt(lua_State* L) {
    luaL_rometatable(L, MQTT_METATABLE, (void*)mqtt_metatable_map);  // create metatable for mqtt
    return 0;
}

NODEMCU_MODULE(MQTT, "mqtt", mqtt_map, luaopen_mqtt);
