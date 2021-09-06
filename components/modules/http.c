#include "module.h"
#include "common.h"
#include "lauxlib.h"
#include "lmem.h"
#include <string.h>

#include "esp_http_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task.h"
#include "task/task.h"

#include <esp_log.h>
#define TAG "http"

typedef struct list_item list_item;

enum {
  ConnectionCallback = 0,
  HeadersCallback,
  DataCallback,
  CompleteCallback,

  // after this are other refs which aren't callbacks

  ContextRef,
  BodyDataRef,
  CertRef,
  CountRefs // Must be last
};

static char const * const CALLBACK_NAME[] = {
  "connect",
  "headers",
  "data",
  "complete",
  NULL
};

typedef enum {
  Async = 0,
  Connected = 1,
  InCallback = 2,
  AckPending = 3,
  ShouldCloseInRtosTask = 4,
} Flag;

typedef struct
{
  esp_http_client_handle_t client;
  int refs[CountRefs];
  list_item *headers;
  int16_t status_code;
  uint16_t flags;
  TaskHandle_t perform_rtos_task; // NULL if we're not in the middle of an async request
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
static const char http_default_user_agent[] = "NodeMCU (ESP32)";
#define DELAY_ACK (-99) // Chosen not to conflict with any other esp_err_t
#define HTTP_REQUEST_COMPLETE (-1)

static task_handle_t lhttp_request_task_id, lhttp_event_task_id;
#define CHECK_CONNECTION_IDLE(ctx) \
  if (ctx->perform_rtos_task || context_flag(ctx, InCallback)) { \
    return luaL_error(L, "Cannot modify connection while a request is active"); \
  }

static void context_setflag(lhttp_context_t *context, Flag flag)
{
  context->flags |= 1 << flag;
}

static void context_clearflag(lhttp_context_t *context, Flag flag)
{
  context->flags &= ~(1 << flag);
}

static bool context_flag(lhttp_context_t const *context, Flag flag)
{
  return context->flags & (1 << flag);
}

static void context_setflagbool(lhttp_context_t *context, Flag flag, bool val)
{
  if (val) {
    context_setflag(context, flag);
  } else {
    context_clearflag(context, flag);
  }
}

static void context_setref(lua_State *L, lhttp_context_t *context, int index)
{
  assert(index >= 0 && index < CountRefs);
  luaL_unref(L, LUA_REGISTRYINDEX, context->refs[index]);
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  if (ref == LUA_REFNIL) {
    ref = LUA_NOREF;
  }
  context->refs[index] = ref;
}

static void context_unsetref(lua_State *L, lhttp_context_t *context, int index)
{
  assert(index >= 0 && index < CountRefs);
  int ref = context->refs[index];
  context->refs[index] = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, ref);
}

static int context_close(lua_State *L)
{
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  if (!context->client) {
    // Nothing to do
  } else if (context->perform_rtos_task) {
    // Can only be closed from within a callback or while there's a delayed ack pending
    if (context_flag(context, InCallback) || context_flag(context, AckPending)) {
      context_setflag(context, ShouldCloseInRtosTask);
    } else {
      return luaL_error(L, "Cannot close an ongoing async request outside of a callback or pending ack");
    }
  } else {
    esp_http_client_close(context->client);
  }

  return 0;
}

static int context_gc(lua_State *L)
{
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  assert(context->refs[ContextRef] == LUA_NOREF); // No way to get GC'd if we still hold a ref to ourselves
  for (int i = 0; i < CountRefs; i++) {
    context_unsetref(L, context, i);
  }
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
  for (int i = 0; i < CountRefs; i++) {
    context->refs[i] = LUA_NOREF;
  }
  context->flags = 0;
  context->perform_rtos_task = NULL;
  luaL_getmetatable(L, http_context_mt);
  lua_setmetatable(L, -2);
  return context;
}

static void context_seterr(lhttp_context_t *context, esp_err_t err)
{
  if (err > 0 && err <= INT16_MAX) {
    context->status_code = -err;
  } else if (err) {
    context->status_code = -1;
  }
}

static void perform_rtos_task(void *pvParameters)
{
  lhttp_context_t *context = (lhttp_context_t *)pvParameters;

  ulTaskNotifyTake(pdTRUE, 0);  // ensure that notification counter is reset
  esp_err_t err = esp_http_client_perform(context->client);
  if (err) {
    ESP_LOGW(TAG, "esp_http_client_perform returned error %d", err);
    context_seterr(context, err);
  }
  ESP_LOGD(TAG, "perform_rtos_task completed, remaining stack = %d bytes", uxTaskGetStackHighWaterMark(NULL));
  task_post_low(lhttp_request_task_id, (task_param_t)context);
  vTaskSuspend(NULL);
}

static int make_callback(lhttp_context_t *context, int id, void *data, size_t data_len);

static void lhttp_request_task(task_param_t param, task_prio_t prio)
{
  lhttp_context_t *context = (lhttp_context_t *)param;

  // the rtos task for esp_http_client_perform suspended, reap it
  vTaskDelete(context->perform_rtos_task);
  context->perform_rtos_task = NULL;

  make_callback(context, HTTP_REQUEST_COMPLETE, NULL, 0);
  lua_State *L = lua_getstate();
  context_unsetref(L, context, ContextRef);
}

// note: this function is called both in synchronous mode and in asynchronous mode
static esp_err_t http_event_cb(esp_http_client_event_t *evt)
{
  // ESP_LOGI(TAG, "http_event_cb %d", evt->event_id);
  lhttp_context_t *context = (lhttp_context_t *)evt->user_data;
  switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED: {
      context_setflag(context, Connected);
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
      // Given when HTTP_EVENT_ON_FINISH is dispatched (before the
      // http_should_keep_alive check in esp_http_client_perform) I don't think
      // there's any benefit to exposing this event
      // int ret = make_callback(context, HTTP_EVENT_ON_FINISH, NULL, 0);
      break;
    }
    case HTTP_EVENT_DISCONNECTED:
      context_clearflag(context, Connected);
      break;
    default:
      break;
  }
  return ESP_OK;
}

// Task posted from http thread when there's an event
static void lhttp_event_task(task_param_t param, task_prio_t prio)
{
  esp_http_client_event_t *evt = (esp_http_client_event_t *)param;
  lhttp_context_t *context = (lhttp_context_t *)evt->user_data;

  context_setflag(context, AckPending);
  int result = http_event_cb(evt);
  if (context->perform_rtos_task && result != DELAY_ACK) {
    context_clearflag(context, AckPending);
    xTaskNotifyGive(context->perform_rtos_task);
  }
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
  lhttp_context_t *context = (lhttp_context_t *)evt->user_data;

  if (context->perform_rtos_task) {
    // asynchronous mode: we're called from perform_rtos_task context
    // 1. post to Lua task
    task_post_high(lhttp_event_task_id, (task_param_t)evt);
    // 2. wait for ack from Lua land
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (context_flag(context, ShouldCloseInRtosTask)) {
      context_clearflag(context, ShouldCloseInRtosTask);
      esp_http_client_close(context->client);
      return ESP_FAIL;
    } else {
      return ESP_OK;
    }
  } else {
    // Call directly
    return http_event_cb(evt);
  }
}

static int make_callback(lhttp_context_t *context, int id, void *data, size_t data_len)
{
  lua_State *L = lua_getstate();
  lua_settop(L, 0);

  switch (id) {
    case HTTP_EVENT_ON_CONNECTED:
      lua_rawgeti(L, LUA_REGISTRYINDEX, context->refs[ConnectionCallback]);
      break;
    case HTTP_EVENT_ON_HEADER: {
      lua_rawgeti(L, LUA_REGISTRYINDEX, context->refs[HeadersCallback]);
      lua_pushinteger(L, context->status_code);
      lua_newtable(L);
      list_item *item = context->headers;
      while (item) {
        lua_pushlstring(L, item->data, item->len); // key
        // Lowercase all header names
        luaL_getmetafield(L, -1, "lower");
        lua_insert(L, -2);
        luaL_pcallx(L, 1, 1);
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
      lua_rawgeti(L, LUA_REGISTRYINDEX, context->refs[DataCallback]);
      lua_pushinteger(L, context->status_code);
      lua_pushlstring(L, data, data_len);
      break;
    case HTTP_REQUEST_COMPLETE:
      lua_rawgeti(L, LUA_REGISTRYINDEX, context->refs[CompleteCallback]);
      lua_pushinteger(L, context->status_code);
      lua_pushboolean(L, context_flag(context, Connected));
      break;
    default:
      break;
  }
  int result = ESP_OK;
  if (lua_type(L, 1) == LUA_TFUNCTION) {
    // Don't set InCallback for complete callback, we use that to determine
    // whether a connection is busy or not, and during a complete it's not
    if (id != HTTP_REQUEST_COMPLETE) {
      context_setflag(context, InCallback);
    }
    int err = luaL_pcallx(L, lua_gettop(L) - 1, 1);
    context_clearflag(context, InCallback);
    if (err) {
      const char *msg = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "<?>";
      ESP_LOGW(TAG, "Error returned from callback for HTTP event %d: %s", id, msg);
      result = ESP_FAIL;
    } else {
      bool delay_ack = (lua_tointeger(L, -1) == DELAY_ACK);
      if (delay_ack) {
        if (!context_flag(context, Async)) {
          luaL_error(L, "Cannot delay acknowledgment of a callback when using synchronous callbacks");
        } else if (id != HTTP_EVENT_ON_DATA) {
          luaL_error(L, "Cannot delay acknowledgment of callbacks other than 'data'");
        }
        result = DELAY_ACK;
      }
    }
  }
  lua_settop(L, 0);
  return result;
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

// Options assumed to be on top of stack
static void parse_options(lua_State *L, lhttp_context_t *context, esp_http_client_config_t *config)
{
  config->timeout_ms = opt_checkint(L, "timeout", 10*1000); // Same default as old http module
  config->buffer_size = opt_checkint(L, "bufsz", DEFAULT_HTTP_BUF_SIZE);
  int redirects = opt_checkint(L, "max_redirects", -1); // -1 means "not specified" here
  if (redirects == 0) {
    config->disable_auto_redirect = true;
  } else if (redirects > 0) {
    config->max_redirection_count = redirects;
  }
  // Note, config->is_async is always set to false regardless of what we set
  // the Async flag to, because of how we configure the tasks we always want
  // esp_http_client_perform to run in its 'is_async=false' mode.
  context_setflagbool(context, Async, opt_checkbool(L, "async", false));

  if (opt_get(L, "cert", LUA_TSTRING)) {
    const char *cert = lua_tostring(L, -1);
    context_setref(L, context, CertRef);
    config->cert_pem = cert;
  }

  // This function doesn't set headers because we need the connection to be created first
}

// http.createConnection([url, [method,] [options])
static int http_lapi_createConnection(lua_State *L)
{
  lua_settop(L, 3);
  const char *url = NULL;
  if (!lua_isnoneornil(L, 1)) {
    url = luaL_checkstring(L, 1);
  }
  int method = HTTP_METHOD_GET;
  if (lua_type(L, 2) == LUA_TNUMBER) {
    method = luaL_checkint(L, 2);
    if (method < 0 || method >= HTTP_METHOD_MAX) {
      return luaL_error(L, "Bad HTTP method %d", method);
    }
  } else {
    // No method, make sure options on top
    lua_settop(L, 2);
  }
  lhttp_context_t *context = context_new(L); // context now on top of stack
  lua_insert(L, -2); // Move context below options

  esp_http_client_config_t config = {
    .url = url,
    .event_handler = http_event_handler,
    .method = method,
    .is_async = false,
    .user_data = context,
  };
  parse_options(L, context, &config);

  context->client = esp_http_client_init(&config);
  if (!context->client) {
    return luaL_error(L, "esp_http_client_init failed");
  }

  // override the default user agent with our own default
  esp_http_client_set_header(context->client, "User-Agent", http_default_user_agent);

  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "headers");
    set_headers(L, lua_gettop(L), context->client);
    lua_pop(L, 1); // headers
  }
  lua_pop(L, 1); // options

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
  context_setref(L, context, callback_idx);
  lua_settop(L, 1);
  return 1; // Return context, for chained calls
}

// context:request()
static int http_lapi_request(lua_State *L)
{
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  CHECK_CONNECTION_IDLE(context);

  // context now owns itself for the duration of the request
  assert(context->refs[ContextRef] == LUA_NOREF);
  lua_pushvalue(L, 1);
  context_setref(L, context, ContextRef);

  if (context_flag(context, Async)) {
    if (xTaskCreate(perform_rtos_task,
                    "http_task",
                    4096,
                    (void *)context,
                    ESP_TASK_MAIN_PRIO + 1,
                    &context->perform_rtos_task) != pdPASS) {
      context_unsetref(L, context, ContextRef);
      return luaL_error(L, "cannot create rtos task");
    }
    return 0;
  } else {
    esp_err_t err = esp_http_client_perform(context->client);
    // Note, the above call invalidates the Lua stack

    if (err) {
      ESP_LOGW(TAG, "Error %d from esp_http_client_perform", err);
      context_seterr(context, err);
    }
    make_callback(context, HTTP_REQUEST_COMPLETE, NULL, 0);

    lua_pushinteger(L, context->status_code);
    lua_pushboolean(L, context_flag(context, Connected));
    context_unsetref(L, context, ContextRef);
    return 2;
  }
}

// connection:seturl(url)
static int http_lapi_seturl(lua_State *L)
{
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  CHECK_CONNECTION_IDLE(context);
  esp_err_t err = esp_http_client_set_url(context->client, luaL_checkstring(L, 2));
  if (err) {
    return luaL_error(L, "esp_http_client_set_url returned %d", err);
  }
  return 0;
}

// connection:setmethod(method)
static int http_lapi_setmethod(lua_State *L)
{
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  CHECK_CONNECTION_IDLE(context);
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
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  CHECK_CONNECTION_IDLE(context);
  const char *name = luaL_checkstring(L, 2);
  const char *value = luaL_optstring(L, 3, NULL);
  esp_http_client_set_header(context->client, name, value);
  return 0;
}

// context:setbody(data)
static int http_lapi_setbody(lua_State *L)
{
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  CHECK_CONNECTION_IDLE(context);

  size_t data_sz;
  const char *data = luaL_optlstring(L, 2, NULL, &data_sz);
  esp_http_client_set_post_field(context->client, data, (int)data_sz);
  context_setref(L, context, BodyDataRef);
  return 0;
}

// context:ack()
static int http_lapi_ack(lua_State *L)
{
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, 1, http_context_mt);
  if (!context_flag(context, AckPending)) {
    return luaL_error(L, "No asynchronous callback pending");
  }
  if (context_flag(context, InCallback)) {
    // Unblocking the HTTP task to potentially trigger more callbacks while
    // we're still processing this callback is just too complicated
    return luaL_error(L, "Cannot call ack() from within the callback itself");
  }
  context_clearflag(context, AckPending);
  xTaskNotifyGive(context->perform_rtos_task);
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

static int http_accumulate_complete(lua_State *L)
{
  lua_settop(L, 1); // Don't care about any of the args except status_code
  int context_idx = lua_upvalueindex(1);
  int cache_table = lua_upvalueindex(2);
  lua_pushvalue(L, lua_upvalueindex(3)); // The callback fn
  lua_insert(L, 1); // Put callback fn first, status_code second

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
    // Remove from table, don't need any more
    lua_pushnil(L);
    lua_rawseti(L, cache_table, i);
  }
  luaL_pushresult(&b); // data now pushed
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, context_idx, http_context_mt);
  if (lua_isnoneornil(L, 1)) {
    // No callback fn so must be sync, meaning just need to stash headers and data in the context
    // steal some completion refs, nothing's going to need them again in a one-shot
    context_setref(L, context, DataCallback); // pops data
    lua_rawgeti(L, cache_table, 2); // headers
    context_setref(L, context, HeadersCallback);
  } else {
    lua_rawgeti(L, cache_table, 2); // headers
    luaL_pcallx(L, 3, 0);
  }
  // unset this since it contains a reference to the context and would prevent the context to be garbage collected
  context_unsetref(L, context,CompleteCallback); 
  return 0;
}

static int make_oneshot_request(lua_State *L, int callback_idx)
{
  // context must be on top of stack
  lhttp_context_t *context = (lhttp_context_t *)luaL_checkudata(L, -1, http_context_mt);
  const bool async = context_flag(context, Async);
  lua_pushvalue(L, -1); // Need this later

  // Make sure we always send Connection: close for oneshots
  esp_http_client_set_header(context->client, "Connection", "close");

  lua_newtable(L); // cache table

  lua_pushvalue(L, -1); // dup cache table
  lua_pushcclosure(L, http_accumulate_headers, 1);
  context_setref(L, context, HeadersCallback);

  lua_pushvalue(L, -1); // dup cache table
  lua_pushcclosure(L, http_accumulate_data, 1);
  context_setref(L, context, DataCallback);

  // Don't dup cache table, it's in the right place on the stack and we don't need it again
  lua_pushvalue(L, callback_idx);
  lua_pushcclosure(L, http_accumulate_complete, 3); // context, cache table, callback
  context_setref(L, context, CompleteCallback);

  // Finally, call request
  lua_pushcfunction(L, http_lapi_request);
  lua_pushvalue(L, -2); // context
  luaL_pcallx(L, 1, 0);

  if (async) {
    return 0;
  } else {
    // Have to return the data here. context is guaranteed still valid because
    // we made sure to keep a reference to it on our stack
    lua_pushinteger(L, context->status_code);
    // Retrieve the data we stashed in context in http_accumulate_complete
    lua_rawgeti(L, LUA_REGISTRYINDEX, context->refs[DataCallback]);
    lua_rawgeti(L, LUA_REGISTRYINDEX, context->refs[HeadersCallback]);
    return 3;
  }
}

// http.get(url, [options,] [callback])
static int http_lapi_get(lua_State *L)
{
  luaL_checkstring(L, 1);
  if (lua_isfunction(L, 2)) {
    lua_pushnil(L);
    lua_insert(L, 2);
  }
  lua_settop(L, 3);
  if (lua_isnil(L, 2)) {
    lua_newtable(L);
    lua_replace(L, 2);
  }
  // Now 1 = url, 2 = non-nil options, 3 = [callback]

  luaL_argcheck(L, lua_istable(L, 2), 2, "options must be nil or a table");
  bool async = lua_isfunction(L, 3);
  luaL_argcheck(L, lua_isnil(L, 3) || async, 3, "callback must be nil or a function");

  // Override options.async based on whether callback present
  lua_pushboolean(L, async);
  lua_setfield(L, 2, "async");

  // Setup call to createConnection
  lua_pushcfunction(L, http_lapi_createConnection);
  lua_pushvalue(L, 1); // url
  lua_pushinteger(L, HTTP_METHOD_GET);
  lua_pushvalue(L, 2); // options

  luaL_pcallx(L, 3, 1); // returns context

  return make_oneshot_request(L, 3);
}

// http.post(url, options, body[, callback])
static int http_lapi_post(lua_State *L)
{
  lua_settop(L, 4);

  luaL_checkstring(L, 1);
  if (lua_isnil(L, 2)) {
    lua_newtable(L);
    lua_replace(L, 2);
  }
  // Now 1 = url, 2 = non-nil options, 3 = body, 4 = [callback]

  luaL_argcheck(L, lua_istable(L, 2), 2, "options must be nil or a table");
  luaL_checkstring(L, 3);
  bool async = lua_isfunction(L, 4);
  luaL_argcheck(L, lua_isnil(L, 4) || async, 4, "callback must be nil or a function");

  // Override options.async based on whether callback present
  lua_pushboolean(L, async);
  lua_setfield(L, 2, "async");

  // Setup call to createConnection
  lua_pushcfunction(L, http_lapi_createConnection);
  lua_pushvalue(L, 1); // url
  lua_pushinteger(L, HTTP_METHOD_POST);
  lua_pushvalue(L, 2); // options

  luaL_pcallx(L, 3, 1); // returns context

  lua_pushcfunction(L, http_lapi_setbody);
  lua_pushvalue(L, -2); // context
  lua_pushvalue(L, 3); // body
  lua_call(L, 2, 0);

  return make_oneshot_request(L, 4); // 4 = callback idx
}

// http.put(url, options, body[, callback])
static int http_lapi_put(lua_State *L)
{
  lua_settop(L, 4);

  luaL_checkstring(L, 1);
  if (lua_isnil(L, 2)) {
    lua_newtable(L);
    lua_replace(L, 2);
  }
  // Now 1 = url, 2 = non-nil options, 3 = body, 4 = [callback]

  luaL_argcheck(L, lua_istable(L, 2), 2, "options must be nil or a table");
  luaL_checkstring(L, 3);
  bool async = lua_isfunction(L, 4);
  luaL_argcheck(L, lua_isnil(L, 4) || async, 4, "callback must be nil or a function");

  // Override options.async based on whether callback present
  lua_pushboolean(L, async);
  lua_setfield(L, 2, "async");

  // Setup call to createConnection
  lua_pushcfunction(L, http_lapi_createConnection);
  lua_pushvalue(L, 1); // url
  lua_pushinteger(L, HTTP_METHOD_PUT);
  lua_pushvalue(L, 2); // options

  lua_call(L, 3, 1); // returns context

  lua_pushcfunction(L, http_lapi_setbody);
  lua_pushvalue(L, -2); // context
  lua_pushvalue(L, 3); // body
  luaL_pcallx(L, 2, 0);

  return make_oneshot_request(L, 4); // 4 = callback idx
}

LROT_BEGIN(http, NULL, 0)
  LROT_FUNCENTRY(createConnection, http_lapi_createConnection)
  LROT_NUMENTRY (GET,              HTTP_METHOD_GET)
  LROT_NUMENTRY (POST,             HTTP_METHOD_POST)
  LROT_NUMENTRY (PUT,              HTTP_METHOD_PUT)
  LROT_NUMENTRY (DELETE,           HTTP_METHOD_DELETE)
  LROT_NUMENTRY (HEAD,             HTTP_METHOD_HEAD)
  LROT_NUMENTRY (DELAYACK,         DELAY_ACK)
  LROT_NUMENTRY (ACKNOW,           0) // Doesn't really matter what this is
  LROT_FUNCENTRY(get,              http_lapi_get)
  LROT_FUNCENTRY(post,             http_lapi_post)
  LROT_FUNCENTRY(put,              http_lapi_put)
LROT_END(http, NULL, 0)

LROT_BEGIN(http_context, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY(__gc,        context_gc)
  LROT_TABENTRY (__index,     http_context)
  LROT_FUNCENTRY(on,          http_lapi_on)
  LROT_FUNCENTRY(request,     http_lapi_request)
  LROT_FUNCENTRY(setmethod,   http_lapi_setmethod)
  LROT_FUNCENTRY(setheader,   http_lapi_setheader)
  LROT_FUNCENTRY(seturl,      http_lapi_seturl)
  LROT_FUNCENTRY(setbody,     http_lapi_setbody)
  LROT_FUNCENTRY(close,       context_close)
  LROT_FUNCENTRY(ack,         http_lapi_ack)
LROT_END(http_context, NULL, LROT_MASK_GC_INDEX)

static int luaopen_http(lua_State *L)
{
  luaL_rometatable(L, http_context_mt, LROT_TABLEREF(http_context));
  lhttp_request_task_id = task_get_id(lhttp_request_task);
  lhttp_event_task_id = task_get_id(lhttp_event_task);
  return 0;
}

NODEMCU_MODULE(HTTP, "http", http, luaopen_http);
