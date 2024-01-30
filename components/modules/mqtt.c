// Module for interfacing with an MQTT broker
#include "esp_log.h"
#include "lauxlib.h"
#include "lmem.h"
#include "lnodeaux.h"
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
#define TAG "MQTT"

// mqtt_context struct contains information to wrap a esp_mqtt client in lua
typedef struct {
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
            lua_ref_t cert_pem;
            lua_ref_t client_cert_pem;
            lua_ref_t client_key_pem;
        };
        lua_ref_t lua_refs[12];
    };
} mqtt_context_t;


typedef struct {
  mqtt_context_t *ctx;
  esp_mqtt_event_t event;
} event_info_t;


// event_handler_t is the function signature for all events
typedef void (*event_handler_t)(lua_State* L, mqtt_context_t* mqtt_context, esp_mqtt_event_handle_t event);

// eventnames contains a list of the events that can be set in lua
// with client:on(eventName, function)
// The order is important, as they map directly to callbacks
// in the union/struct above
const char* const eventnames[] = {"connect", "message", "offline", NULL};

// nodemcu task handlers for receiving events
task_handle_t event_handler_task_id = 0;

// event_clone makes a copy of the mqtt event received so we can pass it on
// and the mqtt library can discard it.
static event_info_t *event_clone(esp_mqtt_event_handle_t ev, mqtt_context_t *ctx) {
    // allocate memory for the copy
    event_info_t *clone = (event_info_t *)malloc(sizeof(event_info_t));
    ESP_LOGD(TAG, "event_clone(): event %p, event id %d, msg %d", ev, ev->event_id, ev->msg_id);
    clone->ctx = ctx;
    esp_mqtt_event_handle_t ev1 = &clone->event;

    // make a shallow copy:
    *ev1 = *ev;

    // if the event carries data, make also a copy of it:
    if (ev->data != NULL) {
        if (ev->data_len > 0) {
            ev1->data = malloc(ev->data_len + 1);  // null-terminate the data, useful for debugging
            memcpy(ev1->data, ev->data, ev->data_len);
            ev1->data[ev1->data_len] = '\0';
            ESP_LOGD(TAG, "event_clone():malloc: event %p, msg %d, data %p, num %d", ev1, ev1->msg_id, ev1->data, ev1->data_len);
        } else {
            ev1->data = NULL;
        }
    }

    // if the event carries a topic, make also a copy of it:
    if (ev->topic != NULL) {
        if (ev->topic_len > 0) {
            ev1->topic = malloc(ev->topic_len + 1);  // null-terminate the data, useful for debugging
            memcpy(ev1->topic, ev->topic, ev->topic_len);
            ev1->topic[ev1->topic_len] = '\0';
            ESP_LOGD(TAG, "event_clone():malloc: event %p, msg %d, topic %p, num %d", ev1, ev1->msg_id, ev1->topic, ev1->topic_len);
        } else {
            ev1->topic = NULL;
        }
    }
    return clone;
}

// event_free deallocates all the memory associated with a cloned event
static void event_free(event_info_t *clone) {
    esp_mqtt_event_handle_t ev = &clone->event;
    if (ev->data != NULL) {
        ESP_LOGD(TAG, "event_free():free: event %p, msg %d, data %p", ev, ev->msg_id, ev->data);
        free(ev->data);
    }
    if (ev->topic != NULL) {
        ESP_LOGD(TAG, "event_free():free: event %p, msg %d, topic %p", ev, ev->msg_id, ev->topic);
        free(ev->topic);
    }
    free(clone);
}

// event_connected is run when the mqtt client connected
static void event_connected(lua_State* L, mqtt_context_t* mqtt_context, esp_mqtt_event_handle_t event) {
    // if the user set a one-shot connected callback, execute it:
    if (luaX_valid_ref(mqtt_context->connected_ok_cb)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->connected_ok_cb);  // push the callback function reference to the stack
        luaX_push_weak_ref(L, mqtt_context->self);                         // push a reference to the client (first parameter)

        ESP_LOGD(TAG, "CB:connect: calling registered one-shot connect callback");
        int res = luaL_pcallx(L, 1, 0);  //call the connect callback: function(client)
        if (res != 0)
            ESP_LOGD(TAG, "CB:connect: Error when calling one-shot connect callback - (%d) %s", res, luaL_checkstring(L, -1));

        //after connecting ok, we clear _both_ the one-shot callbacks:
        luaX_unset_ref(L, &mqtt_context->connected_ok_cb);
        luaX_unset_ref(L, &mqtt_context->connected_nok_cb);
    }

    // now we check for the standard connect callback registered with 'mqtt:on()'
    if (luaX_valid_ref(mqtt_context->on_connect_cb)) {
        ESP_LOGD(TAG, "CB:connect: calling registered standard connect callback");
        lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->on_connect_cb);  // push the callback function reference to the stack
        luaX_push_weak_ref(L, mqtt_context->self);                       // push a reference to the client (first parameter)
        int res = luaL_pcallx(L, 1, 0);                                 //call the connect callback: function(client)
        if (res != 0)
            ESP_LOGD(TAG, "CB:connect: Error when calling connect callback - (%d) %s", res, luaL_checkstring(L, -1));
    }
}

// event_disconnected is run after a connection to the MQTT broker breaks.
static void event_disconnected(lua_State* L, mqtt_context_t* mqtt_context, esp_mqtt_event_handle_t event) {
    if (mqtt_context->client == NULL) {
        ESP_LOGD(TAG, "MQTT Client was NULL on a disconnect event");
    }

    // destroy the wrapped mqtt_client object
    esp_mqtt_client_destroy(mqtt_context->client);
    mqtt_context->client = NULL;

    // if the user set a one-shot connect error callback, execute it:
    if (luaX_valid_ref(mqtt_context->connected_nok_cb)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->connected_nok_cb);  // push the callback function reference to the stack
        luaX_push_weak_ref(L, mqtt_context->self);                          // push a reference to the client (first parameter)
        lua_pushinteger(L, -6);                                             // esp sdk mqtt lib does not provide reason codes. Push "-6" to be backward compatible with ESP8266 API

        ESP_LOGD(TAG, "CB:disconnect: calling registered one-shot disconnect callback");
        int res = luaL_pcallx(L, 2, 0);  //call the disconnect callback with 2 parameters: function(client, reason)
        if (res != 0)
            ESP_LOGD(TAG, "CB:disconnect: Error when calling one-shot disconnect callback - (%d) %s", res, luaL_checkstring(L, -1));

        //after connecting ok, we clear _both_ the one-shot callbacks
        luaX_unset_ref(L, &mqtt_context->connected_ok_cb);
        luaX_unset_ref(L, &mqtt_context->connected_nok_cb);
    }

    // now we check for the standard offline callback registered with 'mqtt:on()'
    if (luaX_valid_ref(mqtt_context->on_offline_cb)) {
        ESP_LOGD(TAG, "CB:disconnect: calling registered standard on_offline_cb callback");
        lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->on_offline_cb);  // push the callback function reference to the stack
        luaX_push_weak_ref(L, mqtt_context->self);                       // push a reference to the client (first parameter)
        int res = luaL_pcallx(L, 1, 0);                                 //call the offline callback: function(client)
        if (res != 0)
            ESP_LOGD(TAG, "CB:disconnect: Error when calling offline callback - (%d) %s", res, luaL_checkstring(L, -1));
    }
}

// event_subscribed is called when the last subscribe call is successful
static void event_subscribed(lua_State* L, mqtt_context_t* mqtt_context, esp_mqtt_event_handle_t event) {
    if (!luaX_valid_ref(mqtt_context->subscribed_ok_cb)) return;

    ESP_LOGD(TAG, "CB:subscribe: calling registered one-shot subscribe callback");
    lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->subscribed_ok_cb);  // push the function reference on the stack
    luaX_push_weak_ref(L, mqtt_context->self);                          // push the client object on the stack
    luaX_unset_ref(L, &mqtt_context->subscribed_ok_cb);                 // forget the callback since it is one-shot
    int res = luaL_pcallx(L, 1, 0);                                    //call the connect callback with one parameter: function(client)
    if (res != 0)
        ESP_LOGD(TAG, "CB:subscribe: Error when calling one-shot subscribe callback - (%d) %s", res, luaL_checkstring(L, -1));
}

//event_published is called when a publish operation completes
static void event_published(lua_State* L, mqtt_context_t* mqtt_context, esp_mqtt_event_handle_t event) {
    if (!luaX_valid_ref(mqtt_context->published_ok_cb)) return;

    ESP_LOGD(TAG, "CB:publish: calling registered one-shot publish callback");
    lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->published_ok_cb);  // push the callback function reference to the stack
    luaX_push_weak_ref(L, mqtt_context->self);                         // push the client reference to the stack
    luaX_unset_ref(L, &mqtt_context->published_ok_cb);                 // forget this callback since it is one-shot
    int res = luaL_pcallx(L, 1, 0);                                   //call the connect callback with 1 parameter: function(client)
    if (res != 0)
        ESP_LOGD(TAG, "CB:publish: Error when calling one-shot publish callback - (%d) %s", res, luaL_checkstring(L, -1));
}

// event_unsubscribed is called when a subscription is successful
static void event_unsubscribed(lua_State* L, mqtt_context_t* mqtt_context, esp_mqtt_event_handle_t event) {
    if (!luaX_valid_ref(mqtt_context->unsubscribed_ok_cb)) return;

    ESP_LOGD(TAG, "CB:unsubscribe: calling registered one-shot unsubscribe callback");
    lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->unsubscribed_ok_cb);  // push callback function reference on the stack
    luaX_push_weak_ref(L, mqtt_context->self);                            // push a reference to the client
    luaX_unset_ref(L, &mqtt_context->unsubscribed_ok_cb);                 // forget callback as it is one-shot
    int res = luaL_pcallx(L, 1, 0);                                      //call the connect callback with one parameter: function(client)
    if (res != 0)
        ESP_LOGD(TAG, "CB:unsubscribe: Error when calling one-shot unsubscribe callback - (%d) %s", res, luaL_checkstring(L, -1));
}

//event_data_received is called when data is received on a subscribed topic
static void event_data_received(lua_State* L, mqtt_context_t* mqtt_context, esp_mqtt_event_handle_t event) {
    if (!luaX_valid_ref(mqtt_context->on_message_cb)) return;

    lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->on_message_cb);
    int numArg = 2;
    luaX_push_weak_ref(L, mqtt_context->self);
    lua_pushlstring(L, event->topic, event->topic_len);
    if (event->data != NULL) {
        lua_pushlstring(L, event->data, event->data_len);
        numArg++;
    }
    int res = luaL_pcallx(L, numArg, 0);  //call the messagecallback
    if (res != 0)
        ESP_LOGD(TAG, "CB:data: Error when calling message callback - (%d) %s", res, luaL_checkstring(L, -1));
}

// event_task_handler takes a nodemcu task message and dispatches it to the appropriate event_xxx callback above.
static void event_task_handler(task_param_t param, task_prio_t prio) {
    // extract the event data out of the task param
    event_info_t *info = (event_info_t *)param;
    esp_mqtt_event_handle_t event = &info->event;
    mqtt_context_t* mqtt_context = info->ctx;

    // Check if this event is about an object that is in the process of garbage collection:
    if (!luaX_valid_ref(mqtt_context->self)) {
        ESP_LOGW(TAG, "caught stray event: %d", event->event_id);  // this can happen if the userdata object is dereferenced while attempting to connect
        goto task_handler_end;                                     // free resources and abort
    }

    lua_State* L = lua_getstate();  //returns main Lua state
    if (L == NULL) {
        goto task_handler_end;  // free resources and abort
    }

    ESP_LOGD(TAG, "event_task_handler: event_id: %d state %p, settings %p, stack top %d", event->event_id, L, mqtt_context, lua_gettop(L));

    event_handler_t eventhandler = NULL;

    switch (event->event_id) {
        case MQTT_EVENT_DATA:
            eventhandler = event_data_received;
            break;
        case MQTT_EVENT_CONNECTED:
            eventhandler = event_connected;
            break;
        case MQTT_EVENT_DISCONNECTED:
            eventhandler = event_disconnected;
            break;
        case MQTT_EVENT_SUBSCRIBED:
            eventhandler = event_subscribed;
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            eventhandler = event_unsubscribed;
            break;
        case MQTT_EVENT_PUBLISHED:
            eventhandler = event_published;
            break;
        default:
            goto task_handler_end;  // free resources and abort
    }

    int top = lua_gettop(L);  // save the stack status to restore it later
    lua_checkstack(L, 5);     // make sure there are at least 5 slots available

    // pin our object by putting a reference on the stack,
    // so it can't be garbage collected during user callback execution.
    luaX_push_weak_ref(L, mqtt_context->self);

    eventhandler(L, mqtt_context, event);

    lua_settop(L, top);  // leave the stack as it was

task_handler_end:
    event_free(info);  // free the cloned event info
}

// mqtt_event_handler receives all events from the esp mqtt library and converts them
// to a task message
static void mqtt_event_handler(void *handler_arg, esp_event_base_t base, int32_t event_id, void *event_data) {
  (void)base;
  (void)event_id;
  event_info_t *clone = event_clone(event_data, handler_arg);
  if (!task_post_medium(event_handler_task_id, (task_param_t)clone))
    event_free(clone);
}

// Lua: on()
// mqtt_on allows to set the callback associated to mqtt events
static int mqtt_on(lua_State* L) {
    if (!lua_isfunction(L, 3))  //check whether we are passed a callback function
        return 0;

    int event = luaL_checkoption(L, 2, "message", eventnames);  // map passed event name to an index in the eventnames array

    mqtt_context_t* mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, MQTT_METATABLE);  //retrieve the mqtt_context

    luaX_set_ref(L, 3, &mqtt_context->lua_refs[event]);  // set the callback reference

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
    const char *host = luaL_checkstring(L, 2);
    bool is_mqtt_uri = strncmp(host, "mqtt://", 7) == 0;
    bool is_mqtts_uri = strncmp(host, "mqtts://", 8) == 0;
    bool is_ws_uri = strncmp(host, "ws://", 5) == 0;
    bool is_wss_uri = strncmp(host, "wss://", 6) == 0;
    if (is_mqtt_uri || is_mqtts_uri || is_ws_uri || is_wss_uri)
      config.broker.address.uri = host;
    else
      config.broker.address.hostname = host;

    // set defaults:
    int secure = 0;
    int reconnect = 0;
    int port = 1883;
    int n = 3;
    const char * cert_pem = NULL;
    const char * client_cert_pem = NULL;
    const char * client_key_pem = NULL;
    size_t cert_pem_len = 0;
    size_t client_cert_pem_len = 0;
    size_t client_key_pem_len = 0;

    if (lua_isnumber(L, n)) {
        if (is_mqtt_uri || is_mqtts_uri)
          return luaL_error(L, "port arg must be nil if giving full uri");
        port = luaL_checknumber(L, n);
        n++;
    } else if (lua_type(L, n) == LUA_TNIL) {
        n++;
    }

    if (lua_isnumber(L, n)) {
        if (is_mqtt_uri || is_mqtts_uri)
          return luaL_error(L, "secure on/off determined by uri");
        secure = !!luaL_checkinteger(L, n);
        n++;

    } else if (lua_type(L, n) == LUA_TNIL) {
        n++;
    } else {
        if (lua_istable(L, n)) {
            secure = true;
            lua_getfield(L, n, "ca_cert");
            if ((cert_pem = luaL_optlstring(L, -1, NULL, &cert_pem_len)) != NULL) {
                luaX_set_ref(L, -1, &mqtt_context->cert_pem);
            }
            lua_pop(L, 1);
            //
            lua_getfield(L, n, "client_cert");
            if ((client_cert_pem = luaL_optlstring(L, -1, NULL, &client_cert_pem_len)) != NULL) {
                luaX_set_ref(L, -1, &mqtt_context->client_cert_pem);
            }
            lua_pop(L, 1);
            //
            lua_getfield(L, n, "client_key");
            if ((client_key_pem = luaL_optlstring(L, -1, NULL, &client_key_pem_len)) != NULL) {
                luaX_set_ref(L, -1, &mqtt_context->client_key_pem);
            }
            lua_pop(L, 1);
            //
            n++;
        }
    }

    if (lua_isnumber(L, n)) {
        reconnect = !!luaL_checkinteger(L, n);
        n++;
    }

    if (lua_isfunction(L, n)) {
        luaX_set_ref(L, n, &mqtt_context->connected_ok_cb);
        n++;
    }

    if (lua_isfunction(L, n)) {
        luaX_set_ref(L, n, &mqtt_context->connected_nok_cb);
        n++;
    }

    ESP_LOGD(TAG, "connect: mqtt_context*: %p", mqtt_context);

    if (config.broker.address.uri == NULL)
    {
      config.broker.address.port = port;
      config.broker.address.transport =
        secure ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;
    }
    config.broker.verification.certificate = cert_pem;
    config.broker.verification.certificate_len = cert_pem_len;
    config.credentials.authentication.certificate = client_cert_pem;
    config.credentials.authentication.certificate_len = client_cert_pem_len;
    config.credentials.authentication.key = client_key_pem;
    config.credentials.authentication.key_len = client_key_pem_len;
    config.credentials.authentication.password = mqtt_context->password;
    config.credentials.client_id = mqtt_context->client_id;
    config.credentials.username = mqtt_context->username;
    config.network.disable_auto_reconnect = (reconnect == 0);
    config.session.disable_clean_session = mqtt_context->disable_clean_session;
    config.session.keepalive = mqtt_context->keepalive;
    config.session.last_will.msg = mqtt_context->lwt_msg;
    config.session.last_will.topic = mqtt_context->lwt_topic;

    // create a mqtt client instance
    mqtt_context->client = esp_mqtt_client_init(&config);
    if (mqtt_context->client == NULL) {
        luaL_error(L, "MQTT library failed to start");
        return 0;
    }

   // register the event handler with mqtt_context as the handler arg
   esp_mqtt_client_register_event(
     mqtt_context->client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_context);

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

    ESP_LOGD(TAG, "Closing MQTT client %p", mqtt_context->client);

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
    luaX_free_string(L, mqtt_context->lwt_topic);
    luaX_free_string(L, mqtt_context->lwt_msg);

    // save a copy of topic and message to pass to the client
    // when connecting
    mqtt_context->lwt_topic = luaX_alloc_string(L, 2, MQTT_MAX_LWT_TOPIC);
    mqtt_context->lwt_msg = luaX_alloc_string(L, 3, MQTT_MAX_LWT_MSG);

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

    ESP_LOGD(TAG, "Set LWT topic '%s', qos %d, retain %d",
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
        luaX_set_ref(L, 6, &mqtt_context->published_ok_cb);
    }

    ESP_LOGD(TAG, "MQTT publish client %p, topic %s, %d bytes", client, topic, data_size);
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
        luaX_set_ref(L, 4, &mqtt_context->subscribed_ok_cb);

    ESP_LOGD(TAG, "MQTT subscribe client %p, topic %s", client, topic);

    // We have to cast away the const due to _Generic expression :(
    esp_err_t err = esp_mqtt_client_subscribe(client, (char *)topic, qos);
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
        luaX_set_ref(L, 3, &mqtt_context->unsubscribed_ok_cb);

    ESP_LOGD(TAG, "MQTT unsubscribe client %p, topic %s", client, topic);

    esp_err_t err = esp_mqtt_client_unsubscribe(client, topic);
    lua_pushboolean(L, err == ESP_OK);

    return 1;  // return 1 value: true OK, false error.
}

// mqtt_deleted is called on garbage collection
static int mqtt_delete(lua_State* L) {
    mqtt_context_t* mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, MQTT_METATABLE);

    // forget all callbacks
    for (int i = 0; i < sizeof(mqtt_context->lua_refs) / sizeof(lua_ref_t); i++) {
        luaX_unset_ref(L, &mqtt_context->lua_refs[i]);
    }

    // if there is a client active, shut it down.
    if (mqtt_context->client != NULL) {
        ESP_LOGD(TAG, "stopping MQTT client %p;", mqtt_context);
        // destroy the client. This is a blocking call.
        // If a connection request was ongoing this will block and
        // a disconnect callback could be fired before coming back here.
        esp_mqtt_client_destroy(mqtt_context->client);
    }

    // free all dynamic strings
    luaX_free_string(L, mqtt_context->client_id);
    luaX_free_string(L, mqtt_context->username);
    luaX_free_string(L, mqtt_context->password);
    luaX_free_string(L, mqtt_context->lwt_msg);
    luaX_free_string(L, mqtt_context->lwt_topic);

    ESP_LOGD(TAG, "MQTT client garbage collected");
    return 0;
}

// Lua: mqtt.Client(clientid, keepalive[, username, password, cleansession])
// mqtt_new creates a new instance of our mqtt userdata lua object
static int mqtt_new(lua_State* L) {
    //create a new lua userdata object and initialize to 0.
    mqtt_context_t* mqtt_context = (mqtt_context_t*)lua_newuserdata(L, sizeof(mqtt_context_t));
    memset(mqtt_context, 0, sizeof(mqtt_context_t));

    // initialize all callbacks to LUA_NOREF, indicating they're unset.
    for (int i = 0; i < sizeof(mqtt_context->lua_refs) / sizeof(lua_ref_t); i++) {
        mqtt_context->lua_refs[i] = LUA_NOREF;
    }

    // keep a weak reference to our userdata object so we can pass it as a parameter to user callbacks
    lua_pushvalue(L, -1);
    mqtt_context->self = luaX_weak_ref(L);

    // store the parameters passed:
    mqtt_context->client_id = luaX_alloc_string(L, 1, MQTT_MAX_CLIENT_LEN);
    ESP_LOGD(TAG, "MQTT client id %s", mqtt_context->client_id);

    mqtt_context->keepalive = luaL_checkinteger(L, 2);

    int n = 2;
    if (lua_isstring(L, 3)) {
        mqtt_context->username = luaX_alloc_string(L, 3, MQTT_MAX_USERNAME_LEN);
        n++;
    }

    if (lua_isstring(L, 4)) {
        mqtt_context->password = luaX_alloc_string(L, 4, MQTT_MAX_PASSWORD_LEN);
        n++;
    }

    if (lua_isnumber(L, 5)) {
        mqtt_context->disable_clean_session = (luaL_checknumber(L, 5) == 0);
        n++;
    }

    luaL_getmetatable(L, MQTT_METATABLE);
    lua_setmetatable(L, -2);

    if (event_handler_task_id == 0) {  // if this is the first time, create nodemcu tasks for every event type
        event_handler_task_id = task_get_id(event_task_handler);
    }

    return 1;  //one object returned, the mqtt context wrapped in a lua userdata object
}

// map client methods to functions:
LROT_BEGIN(mqtt_metatable, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY(__gc, mqtt_delete)
  LROT_TABENTRY(__index, mqtt_metatable)
  LROT_FUNCENTRY(connect, mqtt_connect)
  LROT_FUNCENTRY(close, mqtt_close)
  LROT_FUNCENTRY(lwt, mqtt_lwt)
  LROT_FUNCENTRY(publish, mqtt_publish)
  LROT_FUNCENTRY(subscribe, mqtt_subscribe)
  LROT_FUNCENTRY(unsubscribe, mqtt_unsubscribe)
  LROT_FUNCENTRY(on, mqtt_on)
LROT_END(mqtt_metatable, NULL, LROT_MASK_GC_INDEX)

// Module function map
LROT_BEGIN(mqtt, NULL, 0)
  LROT_FUNCENTRY(Client, mqtt_new)
LROT_END(mqtt, NULL, 0)

int luaopen_mqtt(lua_State* L) {
    luaL_rometatable(L, MQTT_METATABLE, LROT_TABLEREF(mqtt_metatable));  // create metatable for mqtt
    return 0;
}

NODEMCU_MODULE(MQTT, "mqtt", mqtt, luaopen_mqtt);
