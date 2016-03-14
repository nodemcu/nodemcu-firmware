/******************************************************************************
* HTTP module for NodeMCU
* vowstar@gmail.com
* 2015-12-29
*******************************************************************************/
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "cpu_esp8266.h"
#include "osapi.h"
#include "httpclient.h"

static int http_callback_registry  = LUA_NOREF;
static lua_State * http_client_L = NULL;

static void http_callback( char * response, int http_status, char * full_response, int content_length )
{

#if defined(HTTPCLIENT_DEBUG_ON)
	HTTPCLIENT_DEBUG( "http_status=%d\n", http_status );
	
	if ( http_status != HTTP_STATUS_GENERIC_ERROR )
	{
		HTTPCLIENT_DEBUG( "content_length=%d\n", content_length);
		HTTPCLIENT_DEBUG( "strlen(full_response)=%d\n", strlen( full_response ) );
		HTTPCLIENT_DEBUG( "response=%s<EOF>\n", response );
	}
#endif

	if (http_callback_registry != LUA_NOREF)
	{
		lua_rawgeti(http_client_L, LUA_REGISTRYINDEX, http_callback_registry);
		lua_pushnumber(http_client_L, http_status);
		
		if ( http_status != HTTP_STATUS_GENERIC_ERROR )
		{
			lua_pushlstring(http_client_L, response, content_length);

			char *endOfHeader = os_strstr( full_response, "\r\n\r\n" );			
			
			if (endOfHeader != NULL)
			{
				lua_pushlstring(http_client_L, full_response, endOfHeader - full_response);
			}
			else
			{
				// Malformed response, so push nothing instead of pushing full response of unknown content
				lua_pushnil(http_client_L);
			}
		}
		else
		{
			lua_pushnil(http_client_L);
			lua_pushnil(http_client_L);
		}
		
		lua_call(http_client_L, 3, 0); // With 3 arguments and 0 result
		luaL_unref(http_client_L, LUA_REGISTRYINDEX, http_callback_registry);
		http_callback_registry = LUA_NOREF;
	}
}

static int http_lapi_prepare_and_execute(lua_State *L, const char * reqMethod)
{
	int length;
	int argIndex	 	= 0;
	http_client_L 		= L;
	
	// URL
	const char * url = luaL_checklstring(L, ++argIndex, &length);
	luaL_argcheck(L, length > 0, argIndex, "URL not provided");	
	
	// Method
	const char * method = reqMethod;
	if (method == NULL)
	{
		method = luaL_checklstring(L, ++argIndex, &length);
		luaL_argcheck(L, length > 0, argIndex, "Method not provided");
	}

	// Headers
	const char * headers = NULL;
	if (lua_isstring(L, ++argIndex))
	{
		headers = luaL_checklstring(L, argIndex, &length);
	}
	
	// Body (not for GET)
	const char * body = NULL;
	if ((os_strcmp(method, "GET") != 0) && (os_strcmp(method, "HEAD") != 0))
	{
		if (lua_isstring(L, ++argIndex))
		{
			body = luaL_checklstring(L, argIndex, &length);
		}
	}
	
	// Callback
	argIndex++;
	if (lua_type(L, argIndex) == LUA_TFUNCTION || lua_type(L, argIndex) == LUA_TLIGHTFUNCTION) 
	{
		lua_pushvalue(L, argIndex);  // copy argument (func) to the top of stack
		
		if (http_callback_registry != LUA_NOREF)
		{
			luaL_unref(L, LUA_REGISTRYINDEX, http_callback_registry);
		}
		
		http_callback_registry = luaL_ref(L, LUA_REGISTRYINDEX);
	}
	
	http_request(url, method, headers, body, http_callback);
	return 0;	
}

// Lua: http.request( url, method, header, body, function(status, reponse, headers) end )
static int http_lapi_request( lua_State *L )
{
	return http_lapi_prepare_and_execute( L, NULL);
}


// Lua: http.post( url, header, body, function(status, reponse, headers) end )
static int http_lapi_post( lua_State *L )
{
	return http_lapi_prepare_and_execute( L, "POST");
}

// Lua: http.put( url, header, body, function(status, reponse, headers) end )
static int http_lapi_put( lua_State *L )
{
	return http_lapi_prepare_and_execute( L, "PUT");
}

// Lua: http.delete( url, header, body, function(status, reponse, headers) end )
static int http_lapi_delete( lua_State *L )
{
	return http_lapi_prepare_and_execute( L, "DELETE");
}

// Lua: http.get( url, header, function(status, reponse, headers) end )
static int http_lapi_get( lua_State *L )
{
	return http_lapi_prepare_and_execute( L, "GET");
}

// Lua: http.head( url, header, function(status, reponse, headers) end )
static int http_lapi_head( lua_State *L )
{
	return http_lapi_prepare_and_execute( L, "HEAD");
}

// Module function map
static const LUA_REG_TYPE http_map[] = {
{ LSTRKEY( "request" ),         LFUNCVAL( http_lapi_request ) },
{ LSTRKEY( "post" ),            LFUNCVAL( http_lapi_post ) },
{ LSTRKEY( "put" ),             LFUNCVAL( http_lapi_put ) },
{ LSTRKEY( "delete" ),          LFUNCVAL( http_lapi_delete ) },
{ LSTRKEY( "get" ),             LFUNCVAL( http_lapi_get ) },
{ LSTRKEY( "head" ),            LFUNCVAL( http_lapi_head ) },

{ LSTRKEY( "OK" ),              LNUMVAL( 0 ) },
{ LSTRKEY( "ERROR" ),           LNUMVAL( HTTP_STATUS_GENERIC_ERROR ) },

{ LNILKEY, LNILVAL }
};

NODEMCU_MODULE(HTTP, "http", http_map, NULL);