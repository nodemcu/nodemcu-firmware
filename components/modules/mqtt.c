// Module for interfacing with an MQTT broker

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "task/task.h"

#include "mqtt.h"
#include <string.h>

task_handle_t hConn;
task_handle_t hOff;
task_handle_t hPub;
task_handle_t hSub;
task_handle_t hData;

//used as a holder to copy received data from the MQTT task
//to the main Lua task.  Due to the async nature of the tasks
//we must copy the data to deliver it intact to the Lua
//callback
typedef struct lmqtt_ctx
{
	mqtt_client * client;
	char        topic[CONFIG_MQTT_MAX_LWT_TOPIC];
	char        * data;
} lmqtt_ctx_t;


// locate the C mqtt_client pointer and leave the
// Lua instance on the top of the stack
static mqtt_client * get_client( lua_State * L )
{
	if( !lua_istable( L, 1 ) )
	{
		luaL_error( L, "Expected MQTT module (client)" );
		return 0; //never reached
	}

	lua_getfield( L, 1, "_client" );
	if( !lua_islightuserdata( L, -1 ) )
	{
		luaL_error( L, "Expected MQTT client pointer" );
		return 0; //never reached
	}

	mqtt_client * client = (mqtt_client *) lua_touserdata( L, -1 );
	lua_pop( L, 1 ); // just pop the _mqtt field
	return client;
}

// locate the C mqtt_settings pointer and leave the
// Lua instance on the top of the stack
static mqtt_settings * get_settings( lua_State * L )
{
	if( !lua_istable( L, 1 ) )
	{
		luaL_error( L, "Expected MQTT module (settings)" );
		return 0; //never reached
	}

	lua_getfield( L, 1, "_settings" );
	if( !lua_islightuserdata( L, -1 ) )
	{
		luaL_error( L, "Expected MQTT settings pointer" );
		return 0; //never reached
	}

	mqtt_settings * settings = (mqtt_settings *) lua_touserdata( L, -1 );
	lua_pop( L, 1 ); // just pop the _mqtt field
	return settings;
}

// Lua: on()
static int lmqtt_on(lua_State *L)
{
	enum events{
		ON_CONNECT = 0,
		ON_MESSAGE = 1,
		ON_OFFLINE = 2
	};
	const char *const eventnames[] = {"connect", "message", "offline", NULL};

	//  mqtt_settings * settings = get_settings( L );
	int event = luaL_checkoption(L, 2, "message", eventnames);

	if( !lua_isfunction( L, 3 ) )
		return 0;

	switch (event) {
		case ON_CONNECT:
			lua_setfield(L, 1, "_on_connect");
			break;
		case ON_MESSAGE:
			lua_setfield(L, 1, "_on_message");
			break;
		case ON_OFFLINE:
			lua_setfield(L, 1, "_on_offline");
			break;
		default:
			return 0;
	}

	lua_pop(L, 1); //pop event name
	return 0;
}

//typedef void (*task_callback_t)(task_param_t param, task_prio_t prio);
static void lmqtt_connected_cb(task_param_t param, task_prio_t prio) 
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	mqtt_client * client = (mqtt_client *) param;

	int top = lua_gettop(L);
	lua_checkstack(L, 8);

	char key[64];
	snprintf(key, 64, "mqtt_%p", client->settings);
	lua_getglobal( L, key ); //retrieve MQTT table from _G
	NODE_DBG("CB:connect: state %p, settings %p, stack top %d\n", L, client->settings, lua_gettop(L));

	lua_getfield( L, -1, "_connect_ok" );
	if( lua_isfunction( L, -1 ) )
	{
		int top1 = lua_gettop(L);

		NODE_DBG("CB:connect: calling registered one-shot connect callback\n");
		lua_pushvalue( L, -2 ); //dup mqtt table
		int res = lua_pcall( L, 1, 0, 0 ); //call the connect callback
		if( res != 0 )
			NODE_DBG("CB:connect: Error when calling one-shot connect callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );

		//after connecting ok, we clear _both_ the one-shot callbacks
		lua_pushnil(L);
		lua_setfield(L, 1, "_connect_ok");
		lua_pushnil(L);
		lua_setfield(L, 1, "_connect_nok");

		lua_settop(L, top1);
	}

	// now we check for the standard connect callback registered with 'mqtt:on()'
	lua_getfield( L, 1, "_on_connect" );
	if( lua_isfunction( L, -1 ) )
	{
		NODE_DBG("CB:connect: calling registered standard connect callback\n");
		lua_pushvalue( L, 1 ); //dup mqtt table
		int res = lua_pcall( L, 1, 0, 0 ); //call the connect callback
		if( res != 0 )
			NODE_DBG("CB:connect: Error when calling connect callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );
	}
	lua_settop(L, top);
}

static void connected_cb( mqtt_client *client, mqtt_event_data_t *event_data )
{
	task_post_medium(hConn, (task_param_t) client);
}


static void lmqtt_disconnected_cb(task_param_t param, task_prio_t prio)
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	mqtt_client * client = (mqtt_client *) param;

	int top = lua_gettop(L);
	lua_checkstack(L, 8);

	char key[64];
	snprintf(key, 64, "mqtt_%p", client->settings);
	lua_getglobal( L, key ); //retrieve MQTT table from _G
	NODE_DBG("CB:disconnect: state %p, settings %p, stack top %d\n", L, client->settings, lua_gettop(L));

	lua_getfield( L, -1, "_connect_nok" );
	if( lua_isfunction( L, -1 ) )
	{
		NODE_DBG("CB:disconnect: calling registered one-shot disconnect callback\n");
		lua_pushvalue( L, -2 ); //dup mqtt table
		int res = lua_pcall( L, 1, 0, 0 ); //call the disconnect callback
		if( res != 0 )
			NODE_DBG("CB:disconnect: Error when calling one-shot disconnect callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );

		//after connecting ok, we clear _both_ the one-shot callbacks
		lua_pushnil(L);
		lua_setfield(L, 1, "_connect_ok");
		lua_pushnil(L);
		lua_setfield(L, 1, "_connect_nok");
	}

	// now we check for the standard connect callback registered with 'mqtt:on()'
	lua_getfield( L, -1, "_on_offline" );
	if( !lua_isfunction( L, -1 ) || lua_isnil( L, -1 ) )
		return;

	NODE_DBG("CB:disconnect: calling registered standard offline callback\n");
	lua_pushvalue( L, -2 ); //dup mqtt table
	int res = lua_pcall( L, 1, 0, 0 ); //call the offline callback
	if( res != 0 )
		NODE_DBG("CB:disconnect: Error when calling offline callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );

	lua_settop(L, top);
}

static void disconnected_cb( mqtt_client *client, mqtt_event_data_t *event_data )
{
	task_post_medium( hOff, (task_param_t) client);
}

static void lmqtt_subscribe_cb(task_param_t param, task_prio_t prio)
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	mqtt_client * client = (mqtt_client *) param;

	int top = lua_gettop(L);
	lua_checkstack(L, 8);

	char key[64];
	snprintf(key, 64, "mqtt_%p", client->settings);
	lua_getglobal( L, key ); //retrieve MQTT table from _G
	NODE_DBG("CB:subscribe: state %p, settings %p, stack top %d\n", L, client->settings, lua_gettop(L));

	lua_getfield( L, 1, "_subscribe_ok" );
	if( lua_isfunction( L, -1 ) )
	{
		NODE_DBG("CB:subscribe: calling registered one-shot subscribe callback\n");
		lua_pushvalue( L, 1 ); //dup mqtt table
		int res = lua_pcall( L, 1, 0, 0 ); //call the disconnect callback
		if( res != 0 )
			NODE_DBG("CB:subscribe: Error when calling one-shot subscribe callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );

		lua_pushnil(L);
		lua_setfield(L, 1, "_subscribe_ok");
	}
	lua_settop(L, top);
}

static void subscribe_cb( mqtt_client *client, mqtt_event_data_t *event_data )
{
	task_post_medium(hSub, (task_param_t) client);
}

static void lmqtt_publish_cb(task_param_t param, task_prio_t prio)
{
	NODE_DBG("CB:publish: successfully transferred control back to main task\n");
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	mqtt_client * client = (mqtt_client *) param;

	int top = lua_gettop(L);
	lua_checkstack(L, 8);

	char key[64];
	snprintf(key, 64, "mqtt_%p", client->settings);
	lua_getglobal( L, key ); //retrieve MQTT table from _G
	NODE_DBG("CB:publish: state %p, settings %p, stack top %d\n", L, client->settings, lua_gettop(L));

	lua_getfield( L, 1, "_publish_ok" );
	if( lua_isfunction( L, -1 ) )
	{
		NODE_DBG("CB:publish: calling registered one-shot publish callback\n");
		lua_pushvalue( L, 1 ); //dup mqtt table
		int res = lua_pcall( L, 1, 0, 0 ); //call the disconnect callback
		if( res != 0 )
			NODE_DBG("CB:publish: Error when calling one-shot publish callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );

		lua_pushnil(L);
		lua_setfield(L, 1, "_publish_ok");
	}
	lua_settop(L, top);
}

static void publish_cb( mqtt_client *client, mqtt_event_data_t *event_data )
{
	NODE_DBG("CB:publish: transferring control back to main task\n");
	task_post_medium(hPub, (task_param_t) client);
}

static void lmqtt_data_cb(task_param_t param, task_prio_t prio)
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	lmqtt_ctx_t * ctx = (lmqtt_ctx_t *) param;

	int top = lua_gettop(L);
	lua_checkstack(L, 8);

	char key[64];
	snprintf(key, 64, "mqtt_%p", ctx->client->settings);
	lua_getglobal( L, key ); //retrieve MQTT table from _G
	NODE_DBG("CB:data: state %p, settings %p, stack top %d\n", L, ctx->client->settings, lua_gettop(L));

	lua_getfield( L, 1, "_on_message" );
	if( lua_isfunction( L, -1 ) )
	{
		int numArg = 2;
		lua_pushvalue( L, 1 ); //dup mqtt table
		lua_pushstring( L, ctx->topic );
		if( ctx->data != NULL )
		{
			lua_pushstring( L, ctx->data );
			numArg++;
		}

		int res = lua_pcall( L, numArg, 0, 0 ); //call the messagecallback
		if( res != 0 )
			NODE_DBG("CB:data: Error when calling message callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );
	}
	lua_settop(L, top);

	if( ctx->data != NULL )
		free( ctx->data );
}

static void data_cb( mqtt_client *client, mqtt_event_data_t *event_data )
{
	NODE_DBG("CB:data: topic len %d, data len %d\n", event_data->topic_length, event_data->data_length);

	lmqtt_ctx_t * ctx = malloc(sizeof(lmqtt_ctx_t));
	ctx->client = client;
	strncpy(ctx->topic, event_data->topic, event_data->topic_length);
	ctx->topic[event_data->topic_length] = '\0';

	ctx->data = NULL;
	if( event_data->data_length > 0 )
	{
		ctx->data = malloc( event_data->data_length );
		strncpy( ctx->data, event_data->data, event_data->data_length );
		ctx->data[event_data->data_length] = '\0';
	}

	task_post_medium(hData, (task_param_t) ctx);
}


// Lua: mqtt:connect(host[, port[, secure[, autoreconnect]]][, function(client)[, function(client, reason)]])
static int lmqtt_connect( lua_State* L )
{
	mqtt_settings * settings = get_settings( L );

	//  int secure = 0;
	int reconnect = 0;
	const char * host = luaL_checkstring( L, 2 );
	int port = 1883;
	int n = 3;
	if( lua_isnumber( L, n ) )
	{
		port = luaL_checknumber( L, n );
		n++;
	}

	if( lua_isnumber( L, n ) )
	{
		//    secure = !!luaL_checkinteger( L, -4 );
		n++;
	}

	if( lua_isnumber( L, n ) )
	{
		reconnect = !!luaL_checkinteger( L, n );
		n++;
	}

	if( lua_isfunction( L, n ) )
	{
		lua_pushvalue( L, n );
		lua_setfield( L, 1, "_connect_ok" );  // set _G["_cb_connect_ok"] = fn()
		n++;
	}

	if( lua_isfunction( L, n ) )
	{
		lua_pushvalue( L, n );
		lua_setfield(L, 1, "_connect_nok");  // set _G["_cb_connect_nok"] = fn()
		n++;
	}

	lua_pop( L, n - 2 ); //pop parameters

	strncpy(settings->host, host, CONFIG_MQTT_MAX_HOST_LEN );
	settings->port = port;

	settings->auto_reconnect = reconnect != 0;

	settings->connected_cb = connected_cb;
	settings->disconnected_cb = disconnected_cb;
	settings->subscribe_cb = subscribe_cb;
	settings->publish_cb = publish_cb;
	settings->data_cb = data_cb;

	mqtt_client * client = mqtt_start( settings );
	if( client == NULL )
	{
		luaL_error( L, "MQTT library failed to start" );
		return 0;
	}

	NODE_DBG("Created MQTT client @ %p, settings @ %p, state @ %p, top %d \n", client, settings, L, lua_gettop( L ) );
	lua_pushlightuserdata( L, client );
	lua_setfield( L, -2, "_client" ); //and store a reference in the MQTT table

	return 0;
}

// Lua: mqtt:close()
static int lmqtt_close( lua_State* L )
{
	mqtt_client * client = get_client( L );
	if( client == NULL )
		return 0;

	NODE_DBG("Closing MQTT client %p\n", client);

	char id[64];
	snprintf(id, 64, "mqtt_%p", client->settings);
	lua_pushnil( L );
	lua_setglobal( L, id );  // remove global reference

	lua_pushstring( L, "_client" );
	lua_pushnil( L );
	lua_settable( L, -3 ); //and remove a reference in the MQTT table

	return 0;
}

// Lua: mqtt:lwt(topic, message[, qos[, retain]])
static int lmqtt_lwt( lua_State* L )
{
	mqtt_settings * settings = get_settings( L );

	strncpy( settings->lwt_topic, luaL_checkstring( L, 2 ), CONFIG_MQTT_MAX_LWT_TOPIC );
	strncpy( settings->lwt_msg, luaL_checkstring( L, 3 ), CONFIG_MQTT_MAX_LWT_MSG );
	settings->lwt_msg_len = strlen( settings->lwt_msg );

	int n = 4;
	if( lua_isnumber( L, n ) )
	{
		settings->lwt_qos = lua_tonumber( L, n );
		n++;
	}

	if( lua_isnumber( L, n ) )
	{
		settings->lwt_retain = lua_tonumber( L, n );
		n++;
	}

	lua_pop( L, n );
	NODE_DBG("Set LWT topic '%s', qos %d, retain %d, len %d\n", settings->lwt_topic, settings->lwt_qos, settings->lwt_retain, settings->lwt_msg_len);
	return 0;
}

//Lua: mqtt:publish(topic, payload, qos, retain[, function(client)])
static int lmqtt_publish( lua_State * L ) 
{
	mqtt_client * client = get_client( L );

	int top = lua_gettop(L);

	const char * topic = luaL_checkstring( L, 2 );
	const char * data  = luaL_checkstring( L, 3 );
	int qos            = luaL_checkint( L, 4 );
	int retain         = luaL_checkint( L, 5 );

	int n = 6;
	if( lua_isfunction( L, n ) )
	{
		lua_pushvalue( L, n );
		lua_setfield(L, 1, "_publish_ok");  // set _G["_cb_connect_nok"] = fn()
		n++;
	}

	lua_settop(L, top );
	NODE_DBG("MQTT publish client id %s, topic %s, %d bytes\n", client->settings->client_id, topic, strlen(data));
	mqtt_publish(client, topic, data, strlen(data), qos, retain);
	return 0;
}

// Lua: mqtt:subscribe(topic, qos[, function(client)]) OR mqtt:subscribe(table[, function(client)])
static int lmqtt_subscribe( lua_State* L )
{
	mqtt_client * client = get_client( L );

	int top = lua_gettop(L);

	const char * topic = luaL_checkstring( L, 2 );
	int qos            = luaL_checkint( L, 3 );

	if( lua_isfunction( L, 4 ) )
	{
		lua_pushvalue( L, 4 );
		lua_setfield(L, 1, "_subscribe_ok");  // set _G["_cb_connect_nok"] = fn()
	}

	lua_settop(L, top );
	NODE_DBG("MQTT subscribe client id %s, topic %s\n", client->settings->client_id, topic);
	mqtt_subscribe(client, topic, qos);
	return 0;
}

// Lua: mqtt:unsubscribe(topic[, function(client)]) OR mqtt:unsubscribe(table[, function(client)])
static int lmqtt_unsubscribe( lua_State* L )
{
	//  mqtt_client * client = get_client( L );

	return 0;
}

static int lmqtt_delete( lua_State* L )
{
	mqtt_settings * settings = get_settings( L );
	if( settings != NULL )
		free( settings );

	mqtt_client * client = get_client( L );
	if( client != NULL )
	{
		NODE_DBG("stopping MQTT client id %s\n", client->settings->client_id);
		mqtt_destroy( client );
		free( client );
	}
	return 0;
}

// Lua: mqtt.Client(clientid, keepalive[, username, password, cleansession])
static int lmqtt_new( lua_State* L )
{
	const char * clientid = NULL;

	clientid = luaL_checkstring( L, 1 );
	NODE_DBG("MQTT client id %s\n", clientid);

	mqtt_settings * settings = (mqtt_settings *) malloc( sizeof(mqtt_settings) );
	memset(settings, 0, sizeof(mqtt_settings) );

	strncpy(settings->client_id, clientid, CONFIG_MQTT_MAX_CLIENT_LEN);
	settings->keepalive = luaL_checkinteger( L, 2 );

	int n = 2;
	if( lua_isstring(L, 3) )
	{
		strncpy( settings->username, luaL_checkstring( L, 3 ), CONFIG_MQTT_MAX_USERNAME_LEN);
		n++;
	}

	if( lua_isstring(L, 4) )
	{
		strncpy(settings->password, luaL_checkstring( L, 4 ), CONFIG_MQTT_MAX_PASSWORD_LEN);
		n++;
	}

	if( lua_isnumber(L, 5) )
	{
		settings->clean_session = luaL_checknumber( L, 5 );
		n++;
	}
	lua_pop( L, n ); //remove parameters

	lua_newtable( L );
	NODE_DBG("New MQTT table at stack pos %d\n", lua_gettop(L));

	lua_pushlightuserdata( L, settings );
	lua_setfield( L, -2, "_settings" );  // set t["_mqtt"] = client

	lua_pushcfunction( L, lmqtt_connect );
	lua_setfield( L, -2, "connect" );  // set t["connect"] = lmqtt_connect

	lua_pushcfunction( L, lmqtt_close );
	lua_setfield( L, -2, "close" );  // set t["close"] = lmqtt_close

	lua_pushcfunction( L, lmqtt_lwt );
	lua_setfield( L, -2, "lwt" );  // set t["lwt"] = lmqtt_lwt

	lua_pushcfunction( L, lmqtt_publish );
	lua_setfield( L, -2, "publish" );  // set t["publish"] = lmqtt_publish

	lua_pushcfunction( L, lmqtt_subscribe );
	lua_setfield( L, -2, "subscribe" );  // set t["subscribe"] = lmqtt_subscribe

	lua_pushcfunction( L, lmqtt_unsubscribe );
	lua_setfield( L, -2, "unsubscribe" );  // set t["unsubscribe"] = lmqtt_unsubscribe

	lua_pushcfunction( L, lmqtt_on );
	lua_setfield( L, -2, "on" );  // set t["on"] = lmqtt_on

	lua_pushcfunction( L, lmqtt_delete );
	lua_setfield( L, -2, "__gc" );  // set t["__gc"] = lmqtt_delete

	lua_pushvalue( L, 1 ); //make a copy of the table
	lua_setmetatable( L, -2 );

	char id[32];
	snprintf( id, 32, "mqtt_%p", settings );
	NODE_DBG("Store MQTT table in _G stack pos %d\n", lua_gettop(L));
	lua_pushvalue( L, 1 ); //make a copy of the table
	lua_setglobal( L, id);

	hConn = task_get_id(lmqtt_connected_cb);
	hOff  = task_get_id(lmqtt_disconnected_cb);
	hPub  = task_get_id(lmqtt_publish_cb);
	hSub  = task_get_id(lmqtt_subscribe_cb);
	hData = task_get_id(lmqtt_data_cb);

	NODE_DBG("conn %d, off %d, pub %d, sub %d, data %d\n", hConn, hOff, hPub, hSub, hData);
	return 1; //leave table on top of the stack
}


// Module function map
static const LUA_REG_TYPE mqtt_map[] = {
	{ LSTRKEY( "Client" ),	LFUNCVAL( lmqtt_new ) },
	{ LNILKEY, LNILVAL }
};

NODEMCU_MODULE(MQTT, "mqtt", mqtt_map, NULL);
