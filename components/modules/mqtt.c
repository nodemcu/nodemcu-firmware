// Module for interfacing with an MQTT broker

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "task/task.h"

#include "mqtt_client.h"
#include <string.h>

task_handle_t hConn;
task_handle_t hOff;
task_handle_t hPub;
task_handle_t hSub;
task_handle_t hUnsub;
task_handle_t hData;

// ------------------------------------------------------------------------- //

// locate the C mqtt_client pointer and leave the
// Lua instance on the top of the stack
static esp_mqtt_client_handle_t get_client( lua_State * L )
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

	esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) lua_touserdata( L, -1 );
	lua_pop( L, 1 ); // just pop the _mqtt field
	return client;
}

// locate the C mqtt_settings pointer and leave the
// Lua instance on the top of the stack
static esp_mqtt_client_config_t * get_settings( lua_State * L )
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

	esp_mqtt_client_config_t * settings = (esp_mqtt_client_config_t *) lua_touserdata( L, -1 );
	lua_pop( L, 1 ); // just pop the _mqtt field
	return settings;
}

// ------------------------------------------------------------------------- //

static esp_mqtt_event_handle_t event_clone(esp_mqtt_event_handle_t ev)
{
	esp_mqtt_event_handle_t ev1 = (esp_mqtt_event_handle_t) malloc(sizeof(esp_mqtt_event_t));
	memset(ev1, 0, sizeof(esp_mqtt_event_t));
	NODE_DBG("event_clone():malloc: event %p, msg %d\n", ev, ev->msg_id);

	ev1->event_id = ev->event_id;
    ev1->client = ev->client;
    ev1->user_context = ev->user_context;
    ev1->total_data_len = ev->total_data_len;
    ev1->current_data_offset = ev->current_data_offset;
    ev1->msg_id = ev->msg_id;

    ev1->data_len = ev->data_len;
	if( ev->data != NULL && ev->data_len > 0 )
	{
		ev1->data = malloc(ev->data_len + 1);
		memcpy(ev1->data, ev->data, ev->data_len);
		ev1->data[ev1->data_len] = '\0';
		NODE_DBG("event_clone():malloc: event %p, msg %d, data %p, num %d\n", ev1, ev1->msg_id, ev1->data, ev1->data_len);
	}

    ev1->topic_len = ev->topic_len;
	if( ev->topic != NULL && ev->topic_len > 0 )
	{
		ev1->topic = malloc(ev->topic_len + 1);
		memcpy(ev1->topic, ev->topic, ev->topic_len);
		ev1->topic[ev1->topic_len] = '\0';
		NODE_DBG("event_clone():malloc: event %p, msg %d, topic %p, num %d\n", ev1, ev1->msg_id, ev1->topic, ev1->topic_len);
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

//typedef void (*task_callback_t)(task_param_t param, task_prio_t prio);
static void _connected_cb(task_param_t param, task_prio_t prio)
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) param;

	int top = lua_gettop(L);
	lua_checkstack(L, 8);

	char key[64];
	snprintf(key, 64, "mqtt_%p", event->client);
	lua_getglobal( L, key ); //retrieve MQTT table from _G
	NODE_DBG("CB:connect: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));

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
	event_free(event);
}

static void _disconnected_cb(task_param_t param, task_prio_t prio)
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) param;

	int top = lua_gettop(L);
	lua_checkstack(L, 8);

	char key[64];
	snprintf(key, 64, "mqtt_%p", event->client);
	lua_getglobal( L, key ); //retrieve MQTT table from _G
	NODE_DBG("CB:disconnect: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));

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
	{
		event_free(event);
		return;
	}

	NODE_DBG("CB:disconnect: calling registered standard offline callback\n");
	lua_pushvalue( L, -2 ); //dup mqtt table
	int res = lua_pcall( L, 1, 0, 0 ); //call the offline callback
	if( res != 0 )
		NODE_DBG("CB:disconnect: Error when calling offline callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );

	lua_settop(L, top);
	event_free(event);
}

static void _subscribe_cb(task_param_t param, task_prio_t prio)
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) param;

	int top = lua_gettop(L);
	lua_checkstack(L, 8);

	char key[64];
	snprintf(key, 64, "mqtt_%p", event->client);
	lua_getglobal( L, key ); //retrieve MQTT table from _G
	NODE_DBG("CB:subscribe: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));

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
	event_free(event);
}

static void _publish_cb(task_param_t param, task_prio_t prio)
{
	NODE_DBG("CB:publish: successfully transferred control back to main task\n");
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) param;

	int top = lua_gettop(L);
	lua_checkstack(L, 8);

	char key[64];
	snprintf(key, 64, "mqtt_%p", event->client);
	lua_getglobal( L, key ); //retrieve MQTT table from _G
	NODE_DBG("CB:publish: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));

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
	event_free(event);
}

static void _unsubscribe_cb(task_param_t param, task_prio_t prio)
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) param;

	int top = lua_gettop(L);

	char key[64];
	snprintf(key, 64, "mqtt_%p", event->client);
	lua_getglobal( L, key ); //retrieve MQTT table from _G
	NODE_DBG("CB:subscribe: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));

	lua_getfield( L, 1, "_unsubscribe_ok" );
	if( lua_isfunction( L, -1 ) )
	{
		NODE_DBG("CB:unsubscribe: calling registered one-shot unsubscribe callback\n");
		lua_pushvalue( L, 1 ); //dup mqtt table
		int res = lua_pcall( L, 1, 0, 0 ); //call the disconnect callback
		if( res != 0 )
			NODE_DBG("CB:unsubscribe: Error when calling one-shot unsubscribe callback - (%d) %s\n", res, luaL_checkstring( L, -1 ) );

		lua_pushnil(L);
		lua_setfield(L, 1, "_unsubscribe_ok");
	}
	lua_settop(L, top);
	event_free(event);
}

static void _data_cb(task_param_t param, task_prio_t prio)
{
	lua_State * L = lua_getstate(); //returns main Lua state
	if( L == NULL )
		return;

	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) param;

	int top = lua_gettop(L);
	lua_checkstack(L, 8);

	char key[64];
	snprintf(key, 64, "mqtt_%p", event->client);
	lua_getglobal( L, key ); //retrieve MQTT table from _G
	NODE_DBG("CB:data: state %p, settings %p, stack top %d\n", L, event->client, lua_gettop(L));

	lua_getfield( L, 1, "_on_message" );
	if( lua_isfunction( L, -1 ) )
	{
		int numArg = 2;
		lua_pushvalue( L, 1 ); //dup mqtt table
		lua_pushstring( L, event->topic );
		if( event->data != NULL )
		{
			lua_pushstring( L, event->data );
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


// Lua: mqtt:connect(host[, port[, secure[, autoreconnect]]][, function(client)[, function(client, reason)]])
static int mqtt_connect( lua_State* L )
{
	esp_mqtt_client_config_t * mqtt_cfg = get_settings( L );

	int secure = 0;
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

	strncpy(mqtt_cfg->host, host, MQTT_MAX_HOST_LEN );
	mqtt_cfg->port = port;

	mqtt_cfg->disable_auto_reconnect = (reconnect == 0);
	mqtt_cfg->transport = secure ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(mqtt_cfg);
	if( client == NULL )
	{
		luaL_error( L, "MQTT library failed to start" );
		return 0;
	}

    esp_mqtt_client_start(client);

	lua_pushlightuserdata( L, client );
	lua_setfield( L, -2, "_client" ); //and store a reference in the MQTT table

	char id[32];
	snprintf( id, 32, "mqtt_%p", client);
	NODE_DBG("Store MQTT table in _G stack pos %d\n", lua_gettop(L));
	lua_pushvalue( L, 1 ); //make a copy of the table
	lua_setglobal( L, id);

	return 0;
}

// Lua: mqtt:close()
static int mqtt_close( lua_State* L )
{
	esp_mqtt_client_handle_t client = get_client( L );
	if( client == NULL )
		return 0;

	NODE_DBG("Closing MQTT client %p\n", client);

	char id[64];
	snprintf(id, 64, "mqtt_%p", client);
	lua_pushnil( L );
	lua_setglobal( L, id );  // remove global reference

	lua_pushstring( L, "_client" );
	lua_pushnil( L );
	lua_settable( L, -3 ); //and remove a reference in the MQTT table

	return 0;
}

// Lua: mqtt:lwt(topic, message[, qos[, retain]])
static int mqtt_lwt( lua_State* L )
{
	esp_mqtt_client_config_t * mqtt_cfg = get_settings( L );

	strncpy( mqtt_cfg->lwt_topic, luaL_checkstring( L, 2 ), MQTT_MAX_LWT_TOPIC );
	strncpy( mqtt_cfg->lwt_msg, luaL_checkstring( L, 3 ), MQTT_MAX_LWT_MSG );
	mqtt_cfg->lwt_msg_len = strlen( mqtt_cfg->lwt_msg );

	int n = 4;
	if( lua_isnumber( L, n ) )
	{
		mqtt_cfg->lwt_qos = lua_tonumber( L, n );
		n++;
	}

	if( lua_isnumber( L, n ) )
	{
		mqtt_cfg->lwt_retain = lua_tonumber( L, n );
		n++;
	}

	lua_pop( L, n );
	NODE_DBG("Set LWT topic '%s', qos %d, retain %d, len %d\n",
			mqtt_cfg->lwt_topic, mqtt_cfg->lwt_qos, mqtt_cfg->lwt_retain, mqtt_cfg->lwt_msg_len);
	return 0;
}

//Lua: mqtt:publish(topic, payload, qos, retain[, function(client)])
static int mqtt_publish( lua_State * L )
{
	esp_mqtt_client_handle_t client = get_client( L );

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
	NODE_DBG("MQTT publish client %p, topic %s, %d bytes\n", client, topic, strlen(data));
	esp_mqtt_client_publish(client, topic, data, strlen(data), qos, retain);
	return 0;
}

// Lua: mqtt:subscribe(topic, qos[, function(client)]) OR mqtt:subscribe(table[, function(client)])
static int mqtt_subscribe( lua_State* L )
{
	esp_mqtt_client_handle_t client = get_client( L );

	int top = lua_gettop(L);

	const char * topic = luaL_checkstring( L, 2 );
	int qos            = luaL_checkint( L, 3 );

	if( lua_isfunction( L, 4 ) )
	{
		lua_pushvalue( L, 4 );
		lua_setfield(L, 1, "_subscribe_ok");  // set _G["_cb_connect_nok"] = fn()
	}

	lua_settop(L, top );
	NODE_DBG("MQTT subscribe client %p, topic %s\n", client, topic);
	esp_mqtt_client_subscribe(client, topic, qos);
	return 0;
}

// Lua: mqtt:unsubscribe(topic[, function(client)]) OR mqtt:unsubscribe(table[, function(client)])
static int mqtt_unsubscribe( lua_State* L )
{
	esp_mqtt_client_handle_t client = get_client( L );

	int top = lua_gettop(L);

	const char * topic = luaL_checkstring( L, 2 );
	int n = 3;
	if( lua_isfunction( L, n ) )
	{
		lua_pushvalue( L, n );
		lua_setfield(L, 1, "_unsubscribe_ok");  // set _G["_cb_connect_nok"] = fn()
		n++;
	}

	lua_settop(L, top );
	NODE_DBG("MQTT unsubscribe client %p, topic %s\n", client, topic);
	esp_mqtt_client_unsubscribe(client, topic);
	return 0;

	return 0;
}

static int mqtt_delete( lua_State* L )
{
	esp_mqtt_client_config_t * settings = get_settings( L );
	if( settings != NULL )
		free( settings );

	esp_mqtt_client_handle_t client = get_client( L );
	if( client != NULL )
	{
		NODE_DBG("stopping MQTT client %p\n", client);
		esp_mqtt_client_destroy( client );
		free( client );
	}
	return 0;
}

// Lua: mqtt.Client(clientid, keepalive[, username, password, cleansession])
static int mqtt_new( lua_State* L )
{
	const char * clientid = NULL;
	clientid = luaL_checkstring( L, 1 );
	NODE_DBG("MQTT client id %s\n", clientid);

	esp_mqtt_client_config_t * mqtt_cfg = (esp_mqtt_client_config_t *) malloc(sizeof(esp_mqtt_client_config_t));
	memset(mqtt_cfg, 0, sizeof(esp_mqtt_client_config_t));

	mqtt_cfg->event_handle = mqtt_event_handler;

	strncpy(mqtt_cfg->client_id, clientid, MQTT_MAX_CLIENT_LEN);
	mqtt_cfg->keepalive = luaL_checkinteger( L, 2 );

	int n = 2;
	if( lua_isstring(L, 3) )
	{
		strncpy( mqtt_cfg->username, luaL_checkstring( L, 3 ), MQTT_MAX_USERNAME_LEN);
		n++;
	}

	if( lua_isstring(L, 4) )
	{
		strncpy(mqtt_cfg->password, luaL_checkstring( L, 4 ), MQTT_MAX_PASSWORD_LEN);
		n++;
	}

	if( lua_isnumber(L, 5) )
	{
		mqtt_cfg->disable_clean_session = (luaL_checknumber( L, 5 ) == 0);
		n++;
	}
	lua_pop( L, n ); //remove parameters

	lua_newtable( L );
	NODE_DBG("New MQTT table at stack pos %d\n", lua_gettop(L));

	lua_pushlightuserdata( L, mqtt_cfg );
	lua_setfield( L, -2, "_settings" );  // set t["_mqtt"] = client

	luaL_getmetatable( L, "mqtt.mt" );
	lua_setmetatable( L, -2 );

	hConn  = task_get_id(_connected_cb);
	hOff   = task_get_id(_disconnected_cb);
	hPub   = task_get_id(_publish_cb);
	hSub   = task_get_id(_subscribe_cb);
	hUnsub = task_get_id(_unsubscribe_cb);
	hData  = task_get_id(_data_cb);

	NODE_DBG("conn %d, off %d, pub %d, sub %d, data %d\n", hConn, hOff, hPub, hSub, hData);
	return 1; //leave table on top of the stack
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
