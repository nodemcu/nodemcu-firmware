// Module for interfacing with an MQTT broker
#define CONFIG_NODE_DEBUG 1
#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "task/task.h"
#include "lextra.h"

#include "mqtt_client.h"
#include <string.h>

#define MQTT_MAX_HOST_LEN           64
#define MQTT_MAX_CLIENT_LEN         32
#define MQTT_MAX_USERNAME_LEN       32
#define MQTT_MAX_PASSWORD_LEN       65
#define MQTT_MAX_LWT_TOPIC          32
#define MQTT_MAX_LWT_MSG            128

struct mqtt_context {
	esp_mqtt_client_handle_t client;
	char* client_id;
	char* username;
	char* password;
	char* lwt_topic;
	char* lwt_msg;
	int lwt_qos;
	int lwt_retain;
	int keepalive;
	int disable_clean_session;
	int selfref;
	int connect_ok_cb;
	int connect_nok_cb;
	int publish_ok_cb;
	int subscribe_ok_cb;
	int unsubscribe_ok_cb;
	union  {
		struct{
			int connect_cb;
			int message_cb;
			int offline_cb;
		};
		int event_cb[3];
	};
	struct mqtt_context ** pcontext;
};

typedef struct mqtt_context mqtt_context_t;

const char *const eventnames[] = {"connect", "message", "offline", NULL};

task_handle_t hConn = 0;
task_handle_t hOff = 0;
task_handle_t hPub = 0;
task_handle_t hSub = 0;
task_handle_t hUnsub = 0;
task_handle_t hData = 0;


// ------------------------------------------------------------------------- //

static char* alloc_string(lua_State* L, int n, int max_length){
	const char* lua_st = luaL_checkstring( L, n);
	int len = strlen(lua_st);
	if (len > max_length)
		len = max_length;

	char * st = luaM_malloc(L, len+1);
	strncpy(st,lua_st,len);
	st[len] = 0;
	return st;
}

static void free_string(lua_State *L, char* st){
	if(st)
		luaM_freearray(L, st, strlen(st)+1, char);
}

static void unset_ref(lua_State* L, int* ref){
	luaL_unref(L, LUA_REGISTRYINDEX, *ref);
	*ref = LUA_NOREF;
}

static void set_ref(lua_State* L, int n, int* ref){
	unset_ref(L, ref);	
	lua_pushvalue( L, n );	
	*ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

// ------------------------------------------------------------------------- //

static esp_mqtt_event_handle_t event_clone(esp_mqtt_event_handle_t ev)
{
	esp_mqtt_event_handle_t ev1 = (esp_mqtt_event_handle_t) malloc(sizeof(esp_mqtt_event_t));
	NODE_DBG("event_clone():malloc: event %p, msg %d\n", ev, ev->msg_id);
	*ev1 = *ev;

	if( ev->data != NULL) {
		if (ev->data_len > 0 ) {
			ev1->data = malloc(ev->data_len + 1);
			memcpy(ev1->data, ev->data, ev->data_len);
			ev1->data[ev1->data_len] = '\0';
			NODE_DBG("event_clone():malloc: event %p, msg %d, data %p, num %d\n", ev1, ev1->msg_id, ev1->data, ev1->data_len);
		} else {
			ev1->data = NULL;
		}
	}

	if( ev->topic != NULL){
		if (ev->topic_len > 0 ){
			ev1->topic = malloc(ev->topic_len + 1);
			memcpy(ev1->topic, ev->topic, ev->topic_len);
			ev1->topic[ev1->topic_len] = '\0';
			NODE_DBG("event_clone():malloc: event %p, msg %d, topic %p, num %d\n", ev1, ev1->msg_id, ev1->topic, ev1->topic_len);
		}
		else {
			ev1->topic = NULL;
		}
	}
	return ev1;
}

static void event_free(esp_mqtt_event_handle_t ev)
{
	if(ev->data != NULL)
	{
		NODE_DBG("event_free():free: event %p, msg %d, data %p\n", ev, ev->msg_id, ev->data);
		free(ev->data);
	}
	if(ev->topic != NULL)
	{
		NODE_DBG("event_free():free: event %p, msg %d, topic %p\n", ev, ev->msg_id, ev->topic);
		free(ev->topic);
	}
	free(ev);
}

// ------------------------------------------------------------------------- //

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
	mqtt_context_t** pcontext = (mqtt_context_t **) event->user_context;
	NODE_DBG("event_handler: mqtt_context*: %p\n", *pcontext);

	if (*pcontext == NULL) {
		NODE_DBG("caught stray event: %d\n", event->event_id);
		return ESP_OK;
	}

	NODE_DBG("mqtt_event_handler: %d\n",event->event_id);
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
			task_post_medium(hConn, (task_param_t) event_clone(event));
            break;

        case MQTT_EVENT_DISCONNECTED:
			task_post_medium( hOff, (task_param_t) event_clone(event));
            break;

        case MQTT_EVENT_SUBSCRIBED:
			task_post_medium(hSub, (task_param_t) event_clone(event));
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
			task_post_medium(hUnsub, (task_param_t) event_clone(event));
            break;

        case MQTT_EVENT_PUBLISHED:
			task_post_medium(hPub, (task_param_t) event_clone(event));
            break;

        case MQTT_EVENT_DATA:
			task_post_medium(hData, (task_param_t) event_clone(event));
            break;

        case MQTT_EVENT_ERROR:
            break;
    }
    return ESP_OK;
}

static void _connected_cb(task_param_t param, task_prio_t prio)
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) param;
	mqtt_context_t* mqtt_context = *(mqtt_context_t **) event->user_context;

	NODE_DBG("CB:connect: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));
	event_free(event);
	if (mqtt_context->selfref <= 0)
		return;

	int top = lua_gettop(L);
	lua_checkstack(L, 3);

	// pin our object by putting a reference on the stack,
	// so it can't be garbage collected during user callback execution.
	luaL_push_weak_ref(L, mqtt_context->selfref); 

	if(mqtt_context->connect_ok_cb > 0)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->connect_ok_cb);
		luaL_push_weak_ref(L, mqtt_context->selfref);

		NODE_DBG("CB:connect: calling registered one-shot connect callback\n");
		int res = lua_pcall( L, 1, 0, 0 ); //call the connect callback
		if( res != 0 )
			NODE_DBG("CB:connect: Error when calling one-shot connect callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );

		//after connecting ok, we clear _both_ the one-shot callbacks
		unset_ref(L, &mqtt_context->connect_ok_cb);
		unset_ref(L, &mqtt_context->connect_nok_cb);
	}
	lua_settop(L, top);
	
	// now we check for the standard connect callback registered with 'mqtt:on()'
	if( mqtt_context->connect_cb > 0)
	{
		NODE_DBG("CB:connect: calling registered standard connect callback\n");
		lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->connect_cb);
		luaL_push_weak_ref(L, mqtt_context->selfref);
		int res = lua_pcall( L, 1, 0, 0 ); //call the connect callback
		if( res != 0 ) 
			NODE_DBG("CB:connect: Error when calling connect callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );
	}
	lua_settop(L, top);
}

static void _disconnected_cb(task_param_t param, task_prio_t prio)
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) param;
	mqtt_context_t* mqtt_context = *(mqtt_context_t **) event->user_context;
	NODE_DBG("CB:disconnect: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));

	event_free(event);

	if(mqtt_context->client == NULL) {
		NODE_DBG("MQTT Client was NULL on a disconnect event\n");
	}

 	esp_mqtt_client_destroy(mqtt_context->client);
	mqtt_context->client = NULL; 

	if (mqtt_context->selfref <= 0)
		return;

	int top = lua_gettop(L);
	lua_checkstack(L, 4);
	
	// pin our object by putting a reference on the stack,
	// so it can't be garbage collected during user callback execution.
	luaL_push_weak_ref(L, mqtt_context->selfref); 
	
	if(mqtt_context->connect_nok_cb > 0)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->connect_nok_cb);
		luaL_push_weak_ref(L, mqtt_context->selfref);
		lua_pushinteger(L, -6); // esp sdk mqtt lib does not provide reason codes. Push "-6" to be backward compatible.

		NODE_DBG("CB:disconnect: calling registered one-shot disconnect callback\n");
		int res = lua_pcall( L, 2, 0, 0 ); //call the disconnect callback
		if( res != 0 )
			NODE_DBG("CB:disconnect: Error when calling one-shot disconnect callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );

		//after connecting ok, we clear _both_ the one-shot callbacks
		unset_ref(L, &mqtt_context->connect_ok_cb);
		unset_ref(L, &mqtt_context->connect_nok_cb);
	}


	// now we check for the standard offline callback registered with 'mqtt:on()'
	if( mqtt_context->offline_cb > 0)
	{
		NODE_DBG("CB:disconnect: calling registered standard offline_cb callback\n");
		lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->offline_cb);
		luaL_push_weak_ref(L, mqtt_context->selfref);
		int res = lua_pcall( L, 1, 0, 0 ); //call the connect callback
		if( res != 0 ) 
			NODE_DBG("CB:disconnect: Error when calling offline callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );
	}

	lua_settop(L, top);
	NODE_DBG("CB:disconnect exit\n");
}

static void _subscribe_cb(task_param_t param, task_prio_t prio)
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) param;
	mqtt_context_t* mqtt_context = *(mqtt_context_t **) event->user_context;

	NODE_DBG("CB:subscribe: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));
	event_free(event);
	
	if (mqtt_context->selfref <= 0)
		return;

	int top = lua_gettop(L);
	lua_checkstack(L, 3);

	// pin our object by putting a reference on the stack,
	// so it can't be garbage collected during user callback execution.
	luaL_push_weak_ref(L, mqtt_context->selfref); 

	if( mqtt_context->subscribe_ok_cb > 0) {
		NODE_DBG("CB:subscribe: calling registered one-shot subscribe callback\n");
		lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->subscribe_ok_cb);
		luaL_push_weak_ref(L, mqtt_context->selfref);
		int res = lua_pcall( L, 1, 0, 0 ); //call the connect callback
		if( res != 0 ) 
			NODE_DBG("CB:subscribe: Error when calling one-shot subscribe callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );
		
		unset_ref(L, &mqtt_context->subscribe_ok_cb);
	}
	lua_settop(L, top);
}

static void _publish_cb(task_param_t param, task_prio_t prio)
{
	NODE_DBG("CB:publish: successfully transferred control back to main task\n");
	
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) param;
	mqtt_context_t* mqtt_context = *(mqtt_context_t **) event->user_context;

	NODE_DBG("CB:publish: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));
	event_free(event);
	
	if (mqtt_context->selfref <= 0)
		return;

	int top = lua_gettop(L);
	lua_checkstack(L, 3);

	// pin our object by putting a reference on the stack,
	// so it can't be garbage collected during user callback execution.
	luaL_push_weak_ref(L, mqtt_context->selfref); 	

	if( mqtt_context->publish_ok_cb > 0) {
		NODE_DBG("CB:publish: calling registered one-shot publish callback\n");
		lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->publish_ok_cb);
		luaL_push_weak_ref(L, mqtt_context->selfref);
		int res = lua_pcall( L, 1, 0, 0 ); //call the connect callback
		if( res != 0 ) 
			NODE_DBG("CB:publish: Error when calling one-shot publish callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );
		
		unset_ref(L, &mqtt_context->publish_ok_cb);
	}
	lua_settop(L, top);
}

static void _unsubscribe_cb(task_param_t param, task_prio_t prio)
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) param;
	mqtt_context_t* mqtt_context = *(mqtt_context_t **) event->user_context;

	NODE_DBG("CB:unsubscribe: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));
	event_free(event);
	
	if (mqtt_context->selfref <= 0)
		return;

	int top = lua_gettop(L);
	lua_checkstack(L, 3);

	// pin our object by putting a reference on the stack,
	// so it can't be garbage collected during user callback execution.
	luaL_push_weak_ref(L, mqtt_context->selfref); 

	if( mqtt_context->unsubscribe_ok_cb > 0) {
		NODE_DBG("CB:unsubscribe: calling registered one-shot unsubscribe callback\n");
		lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->unsubscribe_ok_cb);
		luaL_push_weak_ref(L, mqtt_context->selfref);
		int res = lua_pcall( L, 1, 0, 0 ); //call the connect callback
		if( res != 0 ) 
			NODE_DBG("CB:unsubscribe: Error when calling one-shot unsubscribe callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );
		
		unset_ref(L, &mqtt_context->unsubscribe_ok_cb);
	}
	lua_settop(L, top);
}

static void _data_cb(task_param_t param, task_prio_t prio)
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) param;
	mqtt_context_t* mqtt_context = *(mqtt_context_t **) event->user_context;

	NODE_DBG("CB:data: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));
	
	if (mqtt_context->selfref <= 0) {
		return;
		event_free(event);
	}

	int top = lua_gettop(L);
	lua_checkstack(L, 5);

	// pin our object by putting a reference on the stack,
	// so it can't be garbage collected during user callback execution.
	luaL_push_weak_ref(L, mqtt_context->selfref); 

	if( mqtt_context->message_cb > 0) {
		lua_rawgeti(L, LUA_REGISTRYINDEX, mqtt_context->message_cb);
		int numArg = 2;
		luaL_push_weak_ref(L, mqtt_context->selfref);
		lua_pushlstring( L, event->topic, event->topic_len);
		if( event->data != NULL )
		{
			lua_pushlstring(L, event->data, event->data_len);
			numArg++;
		}
		int res = lua_pcall( L, numArg, 0, 0 ); //call the messagecallback
		if( res != 0 )
			NODE_DBG("CB:data: Error when calling message callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );
	}
	lua_settop(L, top);
	event_free(event);
}

// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //

// Lua: on()
static int mqtt_on(lua_State *L)
{
	if( !lua_isfunction( L, 3 ) )
		return 0;

	mqtt_context_t * mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, "mqtt.mt");
	int event = luaL_checkoption(L, 2, "message", eventnames);

	set_ref(L, 3, &mqtt_context->event_cb[event]);
	
	return 0;
}


// Lua: mqtt:connect(host[, port[, secure[, autoreconnect]]][, function(client)[, function(client, reason)]])
static int mqtt_connect( lua_State* L )
{
	mqtt_context_t * mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, "mqtt.mt");

	if (mqtt_context->client){
		esp_mqtt_client_destroy(mqtt_context->client);
		mqtt_context->client = NULL;
	}

	esp_mqtt_client_config_t config;
	memset(&config,0, sizeof(esp_mqtt_client_config_t));

	config.host = luaL_checkstring( L, 2);

	int secure = 0;
	int reconnect = 0;
	int port = 1883;
	int n = 3;

	if( lua_isnumber( L, n ) )
	{
		port = luaL_checknumber( L, n );
		n++;
	}

	if( lua_isnumber( L, n ) )
	{
		secure = !!luaL_checkinteger( L, n );
		n++;
	}

	if( lua_isnumber( L, n ) )
	{
		reconnect = !!luaL_checkinteger( L, n );
		n++;
	}

	if( lua_isfunction( L, n ) )
	{
		set_ref(L, n, &mqtt_context->connect_ok_cb);
		n++;
	}

	if( lua_isfunction( L, n ) )
	{
		set_ref(L, n, &mqtt_context->connect_nok_cb);
		n++;
	}

	NODE_DBG("connect: mqtt_context*: %p\n", mqtt_context);
	
	config.user_context = mqtt_context->pcontext;
	config.event_handle = mqtt_event_handler;
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

	mqtt_context->client = esp_mqtt_client_init(&config);
	if( mqtt_context->client == NULL )	{
		luaL_error( L, "MQTT library failed to start" );
		return 0;
	}

	esp_err_t err = esp_mqtt_client_start(mqtt_context->client);
	if (err != ESP_OK){
		luaL_error(L, "Error starting mqtt client");
	}

	return 0;
}

// Lua: mqtt:close()
static int mqtt_close( lua_State* L )
{
	mqtt_context_t * mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, "mqtt.mt");

	if( mqtt_context->client == NULL )
		return 0;

	NODE_DBG("Closing MQTT client %p\n", mqtt_context->client);

	esp_mqtt_client_destroy(mqtt_context->client);
	mqtt_context->client = NULL;

	return 0;
}

// Lua: mqtt:lwt(topic, message[, qos[, retain]])
static int mqtt_lwt( lua_State* L )
{
	mqtt_context_t * mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, "mqtt.mt");
	
	free_string(L, mqtt_context->lwt_topic);
	free_string(L, mqtt_context->lwt_msg);

	mqtt_context->lwt_topic = alloc_string(L, 2, MQTT_MAX_LWT_TOPIC);
	mqtt_context->lwt_msg = alloc_string(L, 3, MQTT_MAX_LWT_MSG);

	int n = 4;
	if( lua_isnumber( L, n ) )
	{
		mqtt_context->lwt_qos = (int)lua_tonumber( L, n );
		n++;
	}

	if( lua_isnumber( L, n ) )
	{
		mqtt_context->lwt_retain = (int)lua_tonumber( L, n );
		n++;
	}

	NODE_DBG("Set LWT topic '%s', qos %d, retain %d\n",
			mqtt_context->lwt_topic, mqtt_context->lwt_qos, mqtt_context->lwt_retain);
	return 0;
}

//Lua: mqtt:publish(topic, payload, qos, retain[, function(client)])
// returns true on success, false otherwise
static int mqtt_publish( lua_State * L )
{
	mqtt_context_t * mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, "mqtt.mt");
	esp_mqtt_client_handle_t client = mqtt_context->client;

	if (client == NULL){
		lua_pushboolean(L, false); // return false (error)
		return 1;
	}

	const char * topic = luaL_checkstring( L, 2 );
	size_t data_size;
	const char * data  = luaL_checklstring(L, 3, &data_size);
	int qos            = luaL_checkint( L, 4 );
	int retain         = luaL_checkint( L, 5 );

	if( lua_isfunction( L, 6 ) )
	{
		set_ref(L, 6, &mqtt_context->publish_ok_cb);
	}

	NODE_DBG("MQTT publish client %p, topic %s, %d bytes\n", client, topic, data_size);
	int msg_id = esp_mqtt_client_publish(client, topic, data, data_size, qos, retain);
	
	lua_pushboolean(L, msg_id >= 0); // if msg_id < 0 there was an error.
	return 1;
}

// Lua: mqtt:subscribe(topic, qos[, function(client)]) OR mqtt:subscribe(table[, function(client)])
// returns true on success, false otherwise
static int mqtt_subscribe( lua_State* L )
{
	mqtt_context_t * mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, "mqtt.mt");
	esp_mqtt_client_handle_t client = mqtt_context->client;

	if (client == NULL){
		lua_pushboolean(L, false); // return false (error)
		return 1;
	}

	const char * topic = luaL_checkstring( L, 2 );
	int qos            = luaL_checkint( L, 3 );

	if( lua_isfunction( L, 4 ) )
		set_ref(L, 4, &mqtt_context->subscribe_ok_cb);

	NODE_DBG("MQTT subscribe client %p, topic %s\n", client, topic);

	esp_err_t err = esp_mqtt_client_subscribe(client, topic, qos);
	lua_pushboolean(L, err == ESP_OK);
	
	return 1;
}

// Lua: mqtt:unsubscribe(topic[, function(client)]) OR mqtt:unsubscribe(table[, function(client)])
// returns true on success, false otherwise
static int mqtt_unsubscribe( lua_State* L )
{
	mqtt_context_t * mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, "mqtt.mt");
	esp_mqtt_client_handle_t client = mqtt_context->client;

	if (client == NULL){
		lua_pushboolean(L, false); // return false (error)
		return 1;
	}

	const char * topic = luaL_checkstring( L, 2 );
	if( lua_isfunction( L, 3 ) )
		set_ref(L, 3, &mqtt_context->unsubscribe_ok_cb);

	NODE_DBG("MQTT unsubscribe client %p, topic %s\n", client, topic);

	esp_err_t err = esp_mqtt_client_unsubscribe(client, topic);
	lua_pushboolean(L, err == ESP_OK);

	return 1;
}

static int mqtt_delete( lua_State* L )
{
	NODE_DBG("mqtt_delete\n");
	mqtt_context_t * mqtt_context = (mqtt_context_t*)luaL_checkudata(L, 1, "mqtt.mt");

	if( mqtt_context->client != NULL )
	{
		NODE_DBG("stopping MQTT client %p; *mqtt_context->pcontext=%p\n", mqtt_context->client, *(mqtt_context->pcontext));
		*(mqtt_context->pcontext) = NULL;
		esp_mqtt_client_destroy(mqtt_context->client);
		NODE_DBG("after destroy. *mqtt_context->pcontext=%p\n", *(mqtt_context->pcontext));
	}

	NODE_DBG("freeing up mem\n");
	luaM_freemem(L, mqtt_context->pcontext, sizeof(mqtt_context_t*));

	unset_ref(L, &mqtt_context->connect_ok_cb);
	unset_ref(L, &mqtt_context->connect_nok_cb);
	unset_ref(L, &mqtt_context->publish_ok_cb);
	unset_ref(L, &mqtt_context->subscribe_ok_cb);
	unset_ref(L, &mqtt_context->unsubscribe_ok_cb);
	unset_ref(L, &mqtt_context->selfref);

	
	for(int i = 0; i<sizeof(mqtt_context->event_cb)/sizeof(int); i++){
		unset_ref(L, &mqtt_context->event_cb[i]);
	}

	free_string(L, mqtt_context->client_id);
	free_string(L, mqtt_context->username);
	free_string(L, mqtt_context->password);
	free_string(L, mqtt_context->lwt_msg);
	free_string(L, mqtt_context->lwt_topic);
	
	NODE_DBG("MQTT client garbage collected\n");

	return 0;
}

// Lua: mqtt.Client(clientid, keepalive[, username, password, cleansession])
static int mqtt_new( lua_State* L )
{
	mqtt_context_t * mqtt_context = (mqtt_context_t*)lua_newuserdata(L,sizeof(mqtt_context_t));
	memset(mqtt_context,0,sizeof(mqtt_context_t));

	mqtt_context->connect_ok_cb = LUA_NOREF;
	mqtt_context->connect_nok_cb = LUA_NOREF;
	mqtt_context->publish_ok_cb = LUA_NOREF;
	mqtt_context->subscribe_ok_cb = LUA_NOREF;
	mqtt_context->unsubscribe_ok_cb = LUA_NOREF;
	for(int i = 0; i<sizeof(mqtt_context->event_cb)/sizeof(int); i++){
		mqtt_context->event_cb[i] = LUA_NOREF;
	}

	lua_pushvalue(L, -1);
	mqtt_context->selfref = luaL_weak_ref(L);

	mqtt_context->pcontext = luaM_malloc(L, sizeof(mqtt_context_t*));
	*(mqtt_context->pcontext) = mqtt_context;
	
	mqtt_context->client_id = alloc_string(L, 1, MQTT_MAX_CLIENT_LEN);
	NODE_DBG("MQTT client id %s\n", mqtt_context->client_id);

	mqtt_context->keepalive = luaL_checkinteger( L, 2 );

	int n = 2;
	if( lua_isstring(L, 3) )
	{
		mqtt_context->username = alloc_string(L, 3, MQTT_MAX_USERNAME_LEN);
		n++;
	}

	if( lua_isstring(L, 4) )
	{
		mqtt_context->password = alloc_string(L, 4, MQTT_MAX_PASSWORD_LEN);
		n++;
	}

	if( lua_isnumber(L, 5) )
	{
		mqtt_context->disable_clean_session = (luaL_checknumber( L, 5 ) == 0);
		n++;
	}

	luaL_getmetatable( L, "mqtt.mt" );
	lua_setmetatable( L, -2 );

	if (hConn == 0) {
		hConn  = task_get_id(_connected_cb);
		hOff   = task_get_id(_disconnected_cb);
		hPub   = task_get_id(_publish_cb);
		hSub   = task_get_id(_subscribe_cb);
		hUnsub = task_get_id(_unsubscribe_cb);
		hData  = task_get_id(_data_cb);
		NODE_DBG("conn %d, off %d, pub %d, sub %d, data %d\n", hConn, hOff, hPub, hSub, hData);
	}

	return 1; //one object returned, the mqtt context wrapped in a lua userdata object
}

static const LUA_REG_TYPE mqtt_metatable_map[] =
{
  { LSTRKEY( "connect" ),     LFUNCVAL( mqtt_connect )},
  { LSTRKEY( "close" ),       LFUNCVAL( mqtt_close )},
  { LSTRKEY( "lwt" ),         LFUNCVAL( mqtt_lwt )},
  { LSTRKEY( "publish" ),     LFUNCVAL( mqtt_publish )},
  { LSTRKEY( "subscribe" ),   LFUNCVAL( mqtt_subscribe )},
  { LSTRKEY( "unsubscribe" ), LFUNCVAL( mqtt_unsubscribe )},
  { LSTRKEY( "on" ),          LFUNCVAL( mqtt_on )},
  { LSTRKEY( "__gc" ),        LFUNCVAL( mqtt_delete )},
  { LSTRKEY( "__index" ),     LROVAL( mqtt_metatable_map )},
  { LNILKEY, LNILVAL}
};

// Module function map
static const LUA_REG_TYPE mqtt_map[] = {
	{ LSTRKEY( "Client" ),	LFUNCVAL( mqtt_new ) },
	{ LNILKEY, LNILVAL }
};

int luaopen_mqtt(lua_State *L) {
  luaL_rometatable(L, "mqtt.mt", (void *)mqtt_metatable_map);  // create metatable for mqtt
  return 0;
}

NODEMCU_MODULE(MQTT, "mqtt", mqtt_map, luaopen_mqtt);
