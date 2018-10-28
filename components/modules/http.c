#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include <string.h>

#include "esp_http_client.h"

#include <esp_log.h>
#define TAG "http"

typedef struct list_item list_item;

enum {
  ConnectionCallback = 0,
  HeadersCallback,
  DataCallback,
  FinishCallback,
  CountCallbacks,
};

static char const * const CALLBACK_NAME[CountCallbacks] = {
  "connect",
  "headers",
  "data",
  "finish",
  NULL
};

typedef struct
{
  esp_http_client_handle_t client;
  esp_http_client_method_t method;
  int callback_ref[CountCallbacks];
  int context_ref;
  int post_data_ref;
  list_item *headers;
  int status_code;
  bool connected;
} lhttp_context_t;

struct list_item
{
  struct list_item *next;
  uint32_t len;
  char data[1];
};

typedef struct
{
  int id;
  lhttp_context_t *context;
  union {
    list_item *headers;
    struct {
      uint32_t data_len;
      char data[1];
    };
  };
} lhttp_event;

static const char http_context_mt[] = "http.context";

static int context_close(lua_State *L)
{
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  if (context->client) {
    esp_http_client_close(context->client);
  }
  return 0;
}

static int context_cleanup(lua_State *L)
{
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  for (int i = 0; i < CountCallbacks; i++) {
    luaL_unref(L, LUA_REGISTRYINDEX, context->callback_ref[i]);
    context->callback_ref[i] = LUA_NOREF;
  }
  luaL_unref(L, LUA_REGISTRYINDEX, context->context_ref);
  context->context_ref = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, context->post_data_ref);
  context->post_data_ref = LUA_NOREF;
  list_item *hdr = context->headers;
  while (hdr) {
    list_item *next = hdr->next;
    free(hdr);
    hdr = next;
  }
  context->headers = NULL;
  if (context->client) {
    esp_http_client_cleanup(context->client);
    context->client = NULL;
  }
  return 0;
}

static lhttp_context_t *context_new(lua_State *L)
{
  lhttp_context_t *context = (lhttp_context_t *)lua_newuserdata(L, sizeof(lhttp_context_t));
  context->client = NULL;
  context->status_code = 0;
  context->headers = NULL;
  context->context_ref = LUA_NOREF;
  context->post_data_ref = LUA_NOREF;
  for (int i = 0; i < CountCallbacks; i++) {
    context->callback_ref[i] = LUA_NOREF;
  }
  context->connected = false;
  luaL_getmetatable(L, http_context_mt);
  lua_setmetatable(L, -2);
  return context;
}

static int make_callback(lhttp_context_t *context, int id, void *data, size_t data_len);

static esp_err_t http_event_cb(esp_http_client_event_t *evt)
{
  // ESP_LOGI(TAG, "http_event_cb %d", evt->event_id);
  lhttp_context_t *context = (lhttp_context_t *)evt->user_data;
  switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED: {
      context->connected = true;
      return make_callback(context, HTTP_EVENT_ON_CONNECTED, NULL, 0);
    }
    case HTTP_EVENT_ON_HEADER: {
      size_t keylen = strlen(evt->header_key);
      size_t vallen = strlen(evt->header_value);
      list_item *hdr = (list_item *)malloc(sizeof(list_item) + keylen + vallen + 1); // +1 for final null
      if (!hdr) {
        return ESP_ERR_NO_MEM;
      }
      hdr->next = context->headers;
      context->headers = hdr;
      hdr->len = keylen;
      memcpy(hdr->data, evt->header_key, keylen + 1);
      memcpy(hdr->data + keylen + 1, evt->header_value, vallen + 1);
      break;
    }
    case HTTP_EVENT_ON_DATA: {
      if (context->headers) {
        context->status_code = esp_http_client_get_status_code(evt->client);
        int err = make_callback(context, HTTP_EVENT_ON_HEADER, NULL, 0);
        if (err) return err;
      }
      return make_callback(context, evt->event_id, evt->data, evt->data_len);
    }
    case HTTP_EVENT_ON_FINISH: {
      if (context->headers) {
        // Might still be set, if there wasn't any data in the request
        context->status_code = esp_http_client_get_status_code(evt->client);
        int err = make_callback(context, HTTP_EVENT_ON_HEADER, NULL, 0);
        if (err) return err;
      }
      int ret = make_callback(context, HTTP_EVENT_ON_FINISH, NULL, 0);
      return ret;
    }
    case HTTP_EVENT_DISCONNECTED:
      context->connected = false;
      break;
    default:
      break;
  }
  return ESP_OK;
}

static int make_callback(lhttp_context_t *context, int id, void *data, size_t data_len)
{
  lua_State *L = lua_getstate();
  lua_settop(L, 0);

  switch (id) {
    case HTTP_EVENT_ON_CONNECTED:
      lua_rawgeti(L, LUA_REGISTRYINDEX, context->callback_ref[ConnectionCallback]);
      break;
    case HTTP_EVENT_ON_HEADER: {
      lua_rawgeti(L, LUA_REGISTRYINDEX, context->callback_ref[HeadersCallback]);
      lua_pushinteger(L, context->status_code);
      lua_newtable(L);
      list_item *item = context->headers;
      while (item) {
        lua_pushlstring(L, item->data, item->len); // key
        // Lowercase all header names
        luaL_getmetafield(L, -1, "lower");
        lua_insert(L, -2);
        lua_call(L, 1, 1);
        char *val = item->data + item->len + 1;
        lua_pushstring(L, val);
        lua_settable(L, -3);
        list_item *next = item->next;
        free(item);
        item = next;
      }
      context->headers = NULL;
      break;
    }
    case HTTP_EVENT_ON_DATA:
      lua_rawgeti(L, LUA_REGISTRYINDEX, context->callback_ref[DataCallback]);
      lua_pushinteger(L, context->status_code);
      lua_pushlstring(L, data, data_len);
      break;
    case HTTP_EVENT_ON_FINISH:
      lua_rawgeti(L, LUA_REGISTRYINDEX, context->callback_ref[FinishCallback]);
      lua_pushinteger(L, context->status_code);
      break;
    default:
      break;
  }
  if (lua_type(L, 1) == LUA_TFUNCTION) {
    int err = lua_pcall(L, lua_gettop(L) - 1, 0, 0);
    if (err) {
      const char *msg = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "<?>";
      ESP_LOGW(TAG, "Error returned from callback for HTTP event %d: %s", id, msg);
      lua_pop(L, 1);
      return ESP_FAIL;
    }
  } else {
    lua_settop(L, 0);
  }
  return ESP_OK;
}

// headers_idx must be absolute idx
static void set_headers(lua_State *L, int headers_idx, esp_http_client_handle_t client)
{
  if (lua_isnoneornil(L, headers_idx)) {
    return;
  }
  lua_pushnil(L);
  while (lua_next(L, headers_idx) != 0) {
    /* uses 'key' (at index -2) and 'value' (at index -1) */
    if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TSTRING) {
      const char *key = lua_tostring(L, -2);
      const char *val = lua_tostring(L, -1);
      esp_http_client_set_header(client, key, val);
    }
    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(L, 1);
  }
}

// http.createConnection([url, [method], [headers])
static int http_lapi_createConnection(lua_State *L)
{
  lua_settop(L, 3);
  const char *url = NULL;
  if (!lua_isnoneornil(L, 1)) {
    url = luaL_checkstring(L, 1);
  }
  int headers_idx = 2;
  int method = HTTP_METHOD_GET;
  if (lua_type(L, 2) == LUA_TNUMBER) {
    method = luaL_checkint(L, 2);
    if (method < 0 || method >= HTTP_METHOD_MAX) {
      return luaL_error(L, "Bad HTTP method %d", method);
    }
    headers_idx = 3;
  }
  lhttp_context_t *context = context_new(L); // context now on top of stack

  esp_http_client_config_t config = {
    .url = url,
    .event_handler = http_event_cb,
    .method = HTTP_METHOD_GET,
    .is_async = false,
    .user_data = context,
  };
  context->client = esp_http_client_init(&config);
  if (!context->client) {
    return luaL_error(L, "esp_http_client_init failed");
  }
  set_headers(L, headers_idx, context->client);
  // Note we do not take a ref to the context itself until http_lapi_request(),
  // because otherwise we'd leak any object that was constructed but never used.
  return 1;
}

// context:on(name, fn)
static int http_lapi_on(lua_State *L)
{
  lua_settop(L, 3);
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  int callback_idx = luaL_checkoption(L, 2, NULL, CALLBACK_NAME);
  luaL_unref(L, LUA_REGISTRYINDEX, context->callback_ref[callback_idx]); // In case of duplicate calls
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  if (ref == LUA_REFNIL) {
    ref = LUA_NOREF;
  }
  context->callback_ref[callback_idx] = ref;
  lua_settop(L, 1);
  return 1; // Return context, for chained calls
}

// context:request()
static int http_lapi_request(lua_State *L)
{
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);

  // context now owns itself for the duration of the connection, if it isn't already
  if (context->context_ref == LUA_NOREF) {
    lua_pushvalue(L, 1);
    context->context_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  esp_err_t err = esp_http_client_perform(context->client);
  // Note, the above call invalidates the Lua stack

  if (err) {
    // There's no way to get an error after sending a FINISH, so we should send
    // it here if there was an error
    ESP_LOGW(TAG, "Error %d from esp_http_client_perform", err);
    context->status_code = -1;
    make_callback(context, HTTP_EVENT_ON_FINISH, NULL, 0);
  }

  lua_pushboolean(L, context->connected);

  int ref = context->context_ref;
  context->context_ref = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, ref);

  return 1;
}

// connection:seturl(url)
static int http_lapi_seturl(lua_State *L)
{
  lua_settop(L, 2);
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  esp_err_t err = esp_http_client_set_url(context->client, luaL_checkstring(L, 2));
  if (err) {
    return luaL_error(L, "esp_http_client_set_url returned %d", err);
  }
  return 0;
}

// connection:setmethod(http.POST)
static int http_lapi_setmethod(lua_State *L)
{
  lua_settop(L, 2);
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  int method = luaL_checkint(L, 2);
  if (method < 0 || method >= HTTP_METHOD_MAX) {
    return luaL_error(L, "Bad HTTP method %d", method);
  }
  esp_http_client_set_method(context->client, method);
  return 0;
}

// connection:setheader(name, val)
static int http_lapi_setheader(lua_State *L)
{
  lua_settop(L, 3);
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  const char *name = luaL_checkstring(L, 2);
  const char *value = luaL_optstring(L, 3, NULL);
  esp_http_client_set_header(context->client, name, value);
  return 0;
}

// context:setpostdata(data)
static int http_lapi_setpostdata(lua_State *L)
{
  lua_settop(L, 2);
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);

  esp_http_client_set_method(context->client, HTTP_METHOD_POST);

  size_t postdata_sz;
  const char *postdata = luaL_optlstring(L, 2, NULL, &postdata_sz);
  esp_http_client_set_post_field(context->client, postdata, (int)postdata_sz);
  luaL_unref(L, LUA_REGISTRYINDEX, context->post_data_ref);
  context->post_data_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}

//// One-shot functions http.get(), http.post() etc follow ////

// args: statusCode, headers
static int http_accumulate_headers(lua_State *L)
{
  int cache_table = lua_upvalueindex(1);
  lua_rawseti(L, cache_table, 2); // cache_table[2] = headers
  lua_rawseti(L, cache_table, 1); // cache_table[1] = statusCode
  lua_pushinteger(L, 2);
  lua_rawseti(L, cache_table, 0); // Use zero for len
  return 0;
}

// args: statusCode, data
static int http_accumulate_data(lua_State *L)
{
  int cache_table = lua_upvalueindex(1);
  lua_rawgeti(L, cache_table, 0);
  int n = lua_tointeger(L, -1);
  lua_pop(L, 1);
  lua_rawseti(L, cache_table, n + 1); // top of stack is data
  lua_pushinteger(L, n + 1);
  lua_rawseti(L, cache_table, 0);
  return 0;
}

static int http_accumulate_finish(lua_State *L)
{
  int cache_table = lua_upvalueindex(1);
  lua_pushvalue(L, lua_upvalueindex(2)); // The callback fn
  lua_rawgeti(L, cache_table, 1); // status

  // Now concat data
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for (int i = 3; ; i++) {
    lua_rawgeti(L, cache_table, i);
    if lua_isnoneornil(L, -1) {
      lua_pop(L, 1);
      break;
    }
    luaL_addvalue(&b);
  }
  luaL_pushresult(&b); // data now pushed
  lua_rawgeti(L, cache_table, 2); // headers
  lua_call(L, 3, 0);
  return 0;
}

static int make_oneshot_request(lua_State *L, int callback_idx)
{
  // context must be on top of stack
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, -1, http_context_mt);

  // Make sure we always send Connection: close for oneshots
  esp_http_client_set_header(context->client, "Connection", "close");

  lua_newtable(L); // cache table

  lua_pushvalue(L, -1); // dup cache table
  lua_pushcclosure(L, http_accumulate_headers, 1);
  context->callback_ref[HeadersCallback] = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_pushvalue(L, -1); // dup cache table
  lua_pushcclosure(L, http_accumulate_data, 1);
  context->callback_ref[DataCallback] = luaL_ref(L, LUA_REGISTRYINDEX);

  // Don't dup cache table, it's in the right place on the stack and we don't need it again
  lua_pushvalue(L, callback_idx);
  lua_pushcclosure(L, http_accumulate_finish, 2);
  context->callback_ref[FinishCallback] = luaL_ref(L, LUA_REGISTRYINDEX);

  // Finally, call request
  lua_pushcfunction(L, http_lapi_request);
  lua_insert(L, -2); // Move fn below context
  lua_call(L, 1, 0);
  return 0;
}

// http.get(url, [headers,] function(status, response, headers) end)
static int http_lapi_get(lua_State *L)
{
  lua_settop(L, 3);
  // Setup call to createConnection
  lua_pushcfunction(L, http_lapi_createConnection);
  lua_pushvalue(L, 1); // url
  lua_pushinteger(L, HTTP_METHOD_GET);
  int callback_idx = 3;
  if (lua_type(L, 2) == LUA_TFUNCTION) {
    // No headers specified
    callback_idx = 2;
    lua_pushnil(L);
  } else {
    lua_pushvalue(L, 2); // headers
  }
  lua_call(L, 3, 1); // returns context

  return make_oneshot_request(L, callback_idx);
}

// http.post(url, headers, body, callback)
static int http_lapi_post(lua_State *L)
{
  lua_settop(L, 4);
  // Setup call to createConnection
  lua_pushcfunction(L, http_lapi_createConnection);
  lua_pushvalue(L, 1); // url
  lua_pushinteger(L, HTTP_METHOD_POST);
  lua_pushvalue(L, 2); // headers
  lua_call(L, 3, 1); // returns context

  lua_pushcfunction(L, http_lapi_setpostdata);
  lua_pushvalue(L, -2); // context
  lua_pushvalue(L, 3); // body
  lua_call(L, 2, 0);

  return make_oneshot_request(L, 4); // 4 = callback idx
}

static const LUA_REG_TYPE http_map[] = {
  { LSTRKEY("createConnection"), LFUNCVAL(http_lapi_createConnection) },
  { LSTRKEY("GET"), LNUMVAL(HTTP_METHOD_GET) },
  { LSTRKEY("POST"), LNUMVAL(HTTP_METHOD_POST) },
  { LSTRKEY("DELETE"), LNUMVAL(HTTP_METHOD_DELETE) },
  { LSTRKEY("HEAD"), LNUMVAL(HTTP_METHOD_HEAD) },
  { LSTRKEY("get"), LFUNCVAL(http_lapi_get) },
  { LSTRKEY("post"), LFUNCVAL(http_lapi_post) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE http_context_map[] = {
  { LSTRKEY("on"), LFUNCVAL(http_lapi_on) },
  { LSTRKEY("request"), LFUNCVAL(http_lapi_request) },
  { LSTRKEY("setmethod"), LFUNCVAL(http_lapi_setmethod) },
  { LSTRKEY("setheader"), LFUNCVAL(http_lapi_setheader) },
  { LSTRKEY("seturl"), LFUNCVAL(http_lapi_seturl) },
  { LSTRKEY("setpostdata"), LFUNCVAL(http_lapi_setpostdata) },
  { LSTRKEY("close"), LFUNCVAL(context_close) },
  { LSTRKEY("__gc"), LFUNCVAL(context_cleanup) },
  { LSTRKEY("__index"), LROVAL(http_context_map) },
  { LNILKEY, LNILVAL }
};

static int luaopen_http(lua_State *L)
{
  luaL_rometatable(L, http_context_mt, (void *)http_context_map);
  return 0;
}

NODEMCU_MODULE(HTTP, "http", http_map, luaopen_http);
