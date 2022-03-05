#include "module.h"
#include "lauxlib.h"

#include "esp_http_server.h"

#include "task/task.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

#include <string.h>
#include <stdio.h>

/**
 * NodeMCU module for interfacing with the esp_http_server component.
 * Said HTTP server runs in its own thread (RTOS task) separate from the
 * LVM thread. As such, running dynamic route handlers in Lua requires a
 * certain amount of thread synchronisation in order to work safely.
 *
 * Effectively, when a dynamic handler is invoked, the functions
 * dynamic_handler_httpd() and dynamic_handler_lvm() will be running in
 * lockstep. The former kicks off the latter with task_post() of the
 * relevant HTTP request information, and then proceeds to servicing
 * requests from dynamic_handler_lvm().
 *
 * There are three things dynamic_handler_lvm() may request from
 * dynamic_handler_httpd():
 *
 *  - Header values. The esp_http_server component provides no method
 *    of enumerating the received headers, so this pull approach is
 *    necssary.
 *
 *  - Body data. In order to facilitate large document bodies the
 *    body is not read in up front, but is instead requested chunk
 *    by chunk from the dynamic handler. This allows for streaming
 *    in e.g. a full OTA image and writing it progressively.
 *
 *  - Sending the response. This includes status message, content type
 *    and any body data. The body data may either be submitted in a single
 *    go, or a function to "pull" the body data chunk by chunk may be
 *    given, in which case chunked encoding is used for the response body
 *    and the content length needs not be known in advance.
 *
 * @author Johny Mattsson (johny.mattsson+github@gmail.com)
 */

// More wieldly names for the Kconfig settings
#define MAX_RESPONSE_HEADERS  CONFIG_NODEMCU_CMODULE_HTTPD_MAX_RESPONSE_HEADERS
#define RECV_BODY_CHUNK_SIZE  CONFIG_NODEMCU_CMODULE_HTTPD_RECV_BODY_CHUNK_SIZE

#define REQUEST_METATABLE "httpd.req"

typedef struct {
  const char *key;
  const char *value;
} key_value_t;


typedef struct {
  const char *status_str; // e.g. "200 OK"
  key_value_t headers[MAX_RESPONSE_HEADERS];
  const char *content_type; // specially handled in esp_http_server
  size_t body_len;
  const char *body_data; // may be binary data, hence body_len above
} response_data_t;


// Request from the LVM thread back to the httpd thread *during* request
// processing in a dynamic handler.
typedef enum {
  GET_HEADER,
  READ_BODY_CHUNK,
  SEND_RESPONSE,
  SEND_PARTIAL_RESPONSE,
} request_type_t;

typedef struct {
  size_t used;
  char data[RECV_BODY_CHUNK_SIZE];
} body_chunk_t;

typedef struct {
  request_type_t request_type;
  union {
    struct {
      const char *name; // owned by LVM thread
      char **value; // allocated in httpd thread, free()d in LVM thread
    } header;
    body_chunk_t **body_chunk; // allocated in httpd thread, free()d in LVM
    const response_data_t *response; // owned by LVM thread
  };
} thread_request_t;


typedef struct {
  const char *key; // dynamic handler lookup key
  const char *uri;
  const char *query_str;
  int method;
  size_t body_len;
} request_data_t;


typedef struct {
  const request_data_t *req_info;
  uint32_t guard;
} req_udata_t;


typedef enum { INDEX_NONE, INDEX_ROOT, INDEX_ALL } index_mode_t;


// Task handle for httpd->LVM thread task posting
static task_handle_t dynamic_task;

// Single-slot queue for passing requests from LVM->httpd thread.
static QueueHandle_t queue;

// Semaphore for releasing the LVM thread once the thread_request has been
// processed by the httpd thread.
static SemaphoreHandle_t done;

// Server instance
static httpd_handle_t server = NULL;

// Path prefix for static files; allocated in LVM thread, used in httpd thread.
// Needed since currently no way to free user_ctx on unregister_uri_handler()
static char *webroot;

// Auto-index mode, configured at server start.
static index_mode_t index_mode;

// Tables for keeping our registered handlers and content type strings
// safe from garbage collection until we want them cleaned up.
static int content_types_table_ref = LUA_NOREF;
static int dynamic_handlers_table_ref = LUA_NOREF;

// Simple guard against deadlocking by calling gethdr()/getbody() outside
// the dynamic handler flow.
static uint32_t guard = 0;

// Known static file suffixes and their content type. Automatically registered
// on server start.
static const char *default_suffixes[] =
{
  "*.html\0text/html",
  "*.css\0text/css",
  "*.js\0text/javascript",
  "*.txt\0text/plain",
  "*.json\0application/json",
  "*.gif\0image/gif",
  "*.jpg\0image/jpeg",
  "*.jpeg\0image/jpeg",
  "*.png\0image/png",
  "*.svg\0image/svg+xml",
  "*.ttf\0font/ttf",
};


// Everybody's favourite response status
static const char internal_err[] = "500 Internal Server Error";

static const response_data_t error_resp = {
  .status_str = internal_err,
  .content_type = "text/plain",
  .body_len = sizeof(internal_err) - 1,
  .body_data = internal_err,
};

// ---- Runs in httpd task/thread -------------------------------------

static bool uri_match_file_suffix_first(const char *uri_template, const char *uri_to_match, size_t match_upto)
{
  if (uri_template[0] == '*')
  {
    // uri_template in form of "*.sufx"
    const char *suffix = uri_template + 1; // skip leading '*'
    size_t suffix_len = strlen(suffix);
    const char *uri_suffix = uri_to_match + match_upto - suffix_len;
    return strncmp(suffix, uri_suffix, suffix_len) == 0;
  }
  else if (uri_template[0] == '\0')
  {
    // auto-indexer template
    switch(index_mode)
    {
      case INDEX_NONE: return false;
      case INDEX_ROOT: return (match_upto == 1) && (uri_to_match[0] == '/');
      case INDEX_ALL: return uri_to_match[match_upto - 1] == '/';
      default: return false;
    }
  }
  else
    return httpd_uri_match_wildcard(uri_template, uri_to_match, match_upto);
}


static void serve_file(httpd_req_t *req, const char *fname)
{
  FILE *f = fopen(fname, "r");
  if (f)
  {
    char *buf = malloc(RECV_BODY_CHUNK_SIZE);
    ssize_t n = 0;
    while ((n = fread(buf, 1, RECV_BODY_CHUNK_SIZE, f)) > 0)
    {
      if (httpd_resp_send_chunk(req, buf, n) != ESP_OK)
        break;
    }
    httpd_resp_send_chunk(req, buf, 0);
    free(buf);
  }
  else
    httpd_resp_send_404(req);

  fclose(f);
}


static esp_err_t static_file_handler(httpd_req_t *req)
{
  httpd_resp_set_type(req, (const char *)req->user_ctx);

  char *fname = NULL;
  asprintf(&fname, "%s%s", webroot, req->uri);
  serve_file(req, fname);
  free(fname);

  return ESP_OK;
};


static esp_err_t auto_index_handler(httpd_req_t *req)
{
  char *fname = NULL;
  asprintf(&fname, "%s%.*s/index.html", webroot, strlen(req->uri) -1, req->uri);
  serve_file(req, fname);
  free(fname);
  return ESP_OK;
}


static esp_err_t dynamic_handler_httpd(httpd_req_t *req)
{
  size_t query_len = httpd_req_get_url_query_len(req);
  char *query = query_len ? malloc(query_len + 1) : NULL;
  if (query_len)
    httpd_req_get_url_query_str(req, query, query_len + 1);

  request_data_t req_data = {
    .key = (const char *)req->user_ctx,
    .uri = req->uri,
    .query_str = query,
    .method = req->method,
    .body_len = req->content_len,
  };
  // Pass the req info over to the LVM thread
  task_post_medium(dynamic_task, (task_param_t)&req_data);

  size_t remaining_len = req->content_len;
  bool errored = false;
  thread_request_t tr;
  do {
    // Block the httpd thread until we receive the response data, or requests
    // for headers/body data, which can only be serviced from this thread.
    xQueueReceive(queue, &tr, portMAX_DELAY);
    if (tr.request_type == GET_HEADER)
    {
      size_t len = httpd_req_get_hdr_value_len(req, tr.header.name);
      if (len)
      {
        *tr.header.value = malloc(len + 1);
        httpd_req_get_hdr_value_str(
          req, tr.header.name, *tr.header.value, len + 1);
      }
      else
        *tr.header.value = NULL; // no such header
    }
    else if (tr.request_type == READ_BODY_CHUNK)
    {
      *tr.body_chunk = malloc(sizeof(body_chunk_t));
      size_t to_read = (remaining_len >= RECV_BODY_CHUNK_SIZE) ?
        RECV_BODY_CHUNK_SIZE : remaining_len;
      remaining_len -= to_read;
      int ret = httpd_req_recv(req, (*tr.body_chunk)->data, to_read);
      if (ret != to_read)
      {
        errored = true;
        free(*tr.body_chunk);
        *tr.body_chunk = NULL;
      }
      else
        (*tr.body_chunk)->used = to_read;
    }
    else if (tr.request_type == SEND_RESPONSE ||
             tr.request_type == SEND_PARTIAL_RESPONSE)
    {
      if (errored)
        httpd_resp_send_408(req);
      else
      {
        bool is_partial = (tr.request_type == SEND_PARTIAL_RESPONSE);
        const response_data_t *resp = tr.response;
        if (!is_partial || resp->status_str)
          httpd_resp_set_status(req, resp->status_str);
        if (!is_partial || resp->content_type)
          httpd_resp_set_type(req, resp->content_type);
        for (unsigned i = 0; resp->headers[i].key; ++i)
          httpd_resp_set_hdr(req, resp->headers[i].key, resp->headers[i].value);
        if (!is_partial)
          httpd_resp_send(req, resp->body_data, resp->body_len);
        else
        {
          httpd_resp_send_chunk(req, resp->body_data, resp->body_len);
          if (resp->body_data == NULL) // Was this the last chunk?
            tr.request_type = SEND_RESPONSE; // If so, flag our exit condition
        }
      }
    }

    // Request processed, release LVM thread
    xSemaphoreGive(done);
  } while(tr.request_type != SEND_RESPONSE); // done

  return ESP_OK;
}


// ---- helper functions ----------------------------------------------


static int check_valid_httpd_method(lua_State *L, int idx)
{
  int method = luaL_checkinteger(L, idx);
  switch (method)
  {
    case HTTP_GET:
    case HTTP_HEAD:
    case HTTP_PUT:
    case HTTP_POST:
    case HTTP_DELETE: break;
    default: return luaL_error(L, "unknown method %d", method);
  }
  return method;
}


static void check_valid_guard_value(lua_State *L)
{
  int check = lua_tointeger(L, lua_upvalueindex(1));
  if (check != guard)
    luaL_error(L, "gethdr()/getbody() called outside synchronous flow");
}


static int lsync_get_hdr(lua_State *L)
{
  check_valid_guard_value(L);

  const char *header_name = luaL_checkstring(L, 2);
  char *header_val = NULL;
  thread_request_t tr = {
    .request_type = GET_HEADER,
    .header = {
      .name = header_name,
      .value = &header_val,
    }
  };
  xQueueSend(queue, &tr, portMAX_DELAY);
  xSemaphoreTake(done, portMAX_DELAY);
  if (header_val)
    lua_pushstring(L, header_val);
  else
    lua_pushnil(L);
  free(header_val);
  return 1;
}


static int lsync_get_body_chunk(lua_State *L)
{
  check_valid_guard_value(L);

  body_chunk_t *chunk = NULL;
  thread_request_t tr = {
    .request_type = READ_BODY_CHUNK,
    .body_chunk = &chunk,
  };
  xQueueSend(queue, &tr, portMAX_DELAY);
  xSemaphoreTake(done, portMAX_DELAY);
  if (chunk)
  {
    if (chunk->used)
      lua_pushlstring(L, chunk->data, chunk->used);
    else
      lua_pushnil(L); // end of body reached
  }
  else
    return luaL_error(L, "read body failed");
  return 1;
}


static int lhttpd_req_index(lua_State *L)
{
  req_udata_t *ud = (req_udata_t *)luaL_checkudata(L, 1, REQUEST_METATABLE);
  const char *key = luaL_checkstring(L, 2);
#define KEY_IS(x) (strcmp(key, x) == 0)
  if (KEY_IS("uri"))
    lua_pushstring(L, ud->req_info->uri);
  else if (KEY_IS("method"))
    lua_pushinteger(L, ud->req_info->method);
  else if (KEY_IS("query") && ud->req_info->query_str)
    lua_pushstring(L, ud->req_info->query_str);
  else if (KEY_IS("headers"))
  {
    lua_newtable(L);
    lua_newtable(L); // metatable
    lua_pushinteger(L, ud->guard); // +1
    lua_pushcclosure(L, lsync_get_hdr, 1); // -1 +1
    lua_setfield(L, -2, "__index"); // -1
    lua_setmetatable(L, -2); // -1
  }
  else if (KEY_IS("getbody"))
  {
    lua_pushinteger(L, guard); // +1
    lua_pushcclosure(L, lsync_get_body_chunk, 1); // -1 +1
  }
  else
    lua_pushnil(L);

  return 1;
#undef KEY_IS
}


static void dynamic_handler_lvm(task_param_t param, task_prio_t prio)
{
  UNUSED(prio);

  const request_data_t *req_info = (const request_data_t *)param;

  lua_State *L = lua_getstate();
  int saved_top = lua_gettop(L);

  lua_checkstack(L, MAX_RESPONSE_HEADERS*2 + 9);

  response_data_t resp = error_resp;
  thread_request_t tr = {
    .request_type = SEND_RESPONSE,
    .response = &resp,
  };

  lua_rawgeti(L, LUA_REGISTRYINDEX, dynamic_handlers_table_ref); // +1
  lua_getfield(L, -1, req_info->key); // +1
  if (lua_isfunction(L, -1))
  {
    // push req
    req_udata_t *ud =
      (req_udata_t *)lua_newuserdata(L, sizeof(req_udata_t)); // +1
    ud->req_info = req_info;
    ud->guard = guard;
    luaL_getmetatable(L, REQUEST_METATABLE); // +1
    lua_setmetatable(L, -2); // -1

    int err = luaL_pcallx(L, 1, 1); // -1 +1
    if (!err && lua_istable(L, -1))
    {
      // pull out response data
      int t = lua_gettop(L); // response table index
      lua_getfield(L, t, "status"); // +1
      resp.status_str = luaL_optstring(L, -1, "200 OK");
      lua_getfield(L, t, "type"); // +1
      resp.content_type = luaL_optstring(L, -1, NULL);
      lua_getfield(L, t, "body"); // +1
      resp.body_data = luaL_optlstring(L, -1, NULL, &resp.body_len);
      if (!resp.body_data)
        resp.body_len = 0;
      lua_getfield(L, t, "headers"); // +1
      if (lua_istable(L, -1))
      {
        lua_pushnil(L); // +1
        for (unsigned i = 0; lua_next(L, -2); ++i) // +1
        {
          if (i >= MAX_RESPONSE_HEADERS)
          {
            printf("Warning - too many response headers, ignoring some!\n");
            break;
          }
          resp.headers[i].key = lua_tostring(L, -2);
          resp.headers[i].value = lua_tostring(L, -1);
          lua_pop(L, 1); // drop value, keep key for lua_next()
        }
      }
      lua_getfield(L, t, "getbody"); // +1
      if (lua_isfunction(L, -1))
      {
        // Okay, we're doing a chunked body send, so we have to repeatedly
        // call the provided getbody() function until it returns nil
        bool headers_cleared = false;
        tr.request_type = SEND_PARTIAL_RESPONSE;
next_chunk:
        resp.body_data = NULL;
        resp.body_len = 0;
        err = luaL_pcallx(L, 0, 1); // -1 +1
        resp.body_data =
          err ? NULL : luaL_optlstring(L, -1, NULL, &resp.body_len);
        if (resp.body_data)
        {
          // Toss this bit of response data over to the httpd thread
          xQueueSend(queue, &tr, portMAX_DELAY);
          // ...and wait until it's done sending it
          xSemaphoreTake(done, portMAX_DELAY);

          lua_pop(L, 1); // -1

          if (!headers_cleared)
          {
            // Clear the header data; it's only used for the first chunk
            resp.status_str = NULL;
            resp.content_type = NULL;
            for (unsigned i = 0; i < MAX_RESPONSE_HEADERS; ++i)
              resp.headers[i].key = resp.headers[i].value = NULL;

            headers_cleared = true;
          }
          lua_getfield(L, t, "getbody"); // +1
          goto next_chunk;
        }
        // else, getbody() returned nil, so let the normal exit path
        // toss the final SEND_PARTIAL_RESPONSE request over to the httpd
      }
    }
  }

  // Toss the response data over to the httpd thread for sending
  xQueueSend(queue, &tr, portMAX_DELAY);

  // Block until the httpd thread has finished accessing our Lua strings
  xSemaphoreTake(done, portMAX_DELAY);

  // Clean up the stack
  lua_settop(L, saved_top);

  // Make any further gethdr()/getbody() calls fail rather than deadlock
  ++guard;
}


// ---- Lua interface -------------------------------------------------


// add static route: httpd.static(uri, content_type)
static int lhttpd_static(lua_State *L)
{
  if (!server)
    return luaL_error(L, "Server not started");

  const char *match = luaL_checkstring(L, 1);
  const char *content_type = luaL_checkstring(L, 2);

  if (!match[0])
    return luaL_error(L, "Null route not supported");

  // Store this in our content-type table, so the content-type string lives
  // on, but so that we can also free it after server shutdown.
  lua_rawgeti(L, LUA_REGISTRYINDEX, content_types_table_ref);
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_settable(L, -3);

  httpd_uri_t static_handler = {
    .uri = match,
    .method = HTTP_GET,
    .handler = static_file_handler,
    .user_ctx = (void *)content_type,
  };
  if (httpd_register_uri_handler(server, &static_handler) == 1)
    lua_pushinteger(L, 1);
  else
    lua_pushnil(L);

  return 1;
}


// add dynamic route: httpd.dynamic(method, uri, handler)
static int lhttpd_dynamic(lua_State *L)
{
  if (!server)
    return luaL_error(L, "Server not started");

  int method = check_valid_httpd_method(L, 1);
  const char *match = luaL_checkstring(L, 2);
  luaL_checkfunction(L, 3);
  lua_settop(L, 3);

  if (!match[0])
    return luaL_error(L, "Null route not supported");

  // Create a key for this entry
  const char *key = lua_pushfstring(L, "[%d]%s", method, match);

  // Store this in our dynamic handlers table, so the ref lives on
  // on, but so that we can also free it after server shutdown.
  lua_rawgeti(L, LUA_REGISTRYINDEX, dynamic_handlers_table_ref);
  lua_pushvalue(L, -2); // key
  lua_pushvalue(L, 3); // handler
  lua_settable(L, -3);

  httpd_uri_t static_handler = {
    .uri = match,
    .method = method,
    .handler = dynamic_handler_httpd,
    .user_ctx = (void *)key,
  };
  if (httpd_register_uri_handler(server, &static_handler) == 1)
    lua_pushinteger(L, 1);
  else
    lua_pushnil(L);

  return 1;
}


// unregister route; httpd.unregister(method, uri)
static int lhttpd_unregister(lua_State *L)
{
  if (!server)
    return luaL_error(L, "Server not started");

  int method = check_valid_httpd_method(L, 1);
  const char *match = luaL_checkstring(L, 2);

  if (httpd_unregister_uri_handler(server, match, method) == ESP_OK)
    lua_pushinteger(L, 1);
  else
    lua_pushnil(L);

  return 1;
}


static int lhttpd_start(lua_State *L)
{
  if (server)
    return luaL_error(L, "Server already started");

  luaL_checktable(L, 1);
  lua_settop(L, 1);

  lua_getfield(L, 1, "webroot");
  const char *root = luaL_checkstring(L, -1);
  webroot = strdup(root);
  lua_pop(L, 1);

  lua_getfield(L, 1, "max_handlers");
  int max_handlers = luaL_optinteger(L, -1, 20);

  lua_getfield(L, 1, "auto_index");
  index_mode = (index_mode_t)luaL_optinteger(L, -1, INDEX_ROOT);

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = uri_match_file_suffix_first;
  config.max_uri_handlers = max_handlers;
  config.max_resp_headers = MAX_RESPONSE_HEADERS;

  esp_err_t err = httpd_start(&server, &config);
  if (err != ESP_OK)
    return luaL_error(L, "Failed to start http server; code %d", err);

  // Set up our content type stash
  lua_newtable(L);
  content_types_table_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  // Set up our dynamic handlers table
  lua_newtable(L);
  dynamic_handlers_table_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  // Register default static suffixes
  size_t num_suffixes = sizeof(default_suffixes)/sizeof(default_suffixes[0]);
  for (size_t i = 0; i < num_suffixes; ++i)
  {
    const char *sufx = default_suffixes[i];
    const char *content_type = strchr(sufx, '\0') + 1;
    lua_pushcfunction(L, lhttpd_static);
    lua_pushstring(L, sufx);
    lua_pushstring(L, content_type);
    lua_call(L, 2, 0);
  }

  // Auto-indexer
  httpd_uri_t index_handler = {
    .uri = "",
    .method = HTTP_GET,
    .handler = auto_index_handler,
  };
  httpd_register_uri_handler(server, &index_handler);

  return 0;
}


static int lhttpd_stop(lua_State *L)
{
  if (server)
  {
    httpd_stop(server); // deletes all handlers
    server = NULL;
    free(webroot);
    luaL_unref2(L, LUA_REGISTRYINDEX, content_types_table_ref);
    luaL_unref2(L, LUA_REGISTRYINDEX, dynamic_handlers_table_ref);
  }
  return 0;
}


LROT_BEGIN(httpd_req_mt, NULL, LROT_MASK_INDEX)
  LROT_FUNCENTRY( __index, lhttpd_req_index )
LROT_END(httpd_req_mt, NULL, LROT_MASK_INDEX)


LROT_BEGIN(httpd, NULL, 0)
  LROT_FUNCENTRY( start,      lhttpd_start )
  LROT_FUNCENTRY( stop,       lhttpd_stop )
  LROT_FUNCENTRY( static,     lhttpd_static )
  LROT_FUNCENTRY( dynamic,    lhttpd_dynamic )
  LROT_FUNCENTRY( unregister, lhttpd_unregister )

  LROT_NUMENTRY( GET,         HTTP_GET )
  LROT_NUMENTRY( HEAD,        HTTP_HEAD )
  LROT_NUMENTRY( PUT,         HTTP_PUT )
  LROT_NUMENTRY( POST,        HTTP_POST )
  LROT_NUMENTRY( DELETE,      HTTP_DELETE )

  LROT_NUMENTRY( INDEX_NONE,  INDEX_NONE )
  LROT_NUMENTRY( INDEX_ROOT,  INDEX_ROOT )
  LROT_NUMENTRY( INDEX_ALL,   INDEX_ALL  )
LROT_END(httpd, NULL, 0)


static int lhttpd_init(lua_State *L)
{
  dynamic_task = task_get_id(dynamic_handler_lvm);
  queue = xQueueCreate(1, sizeof(thread_request_t));
  done = xSemaphoreCreateBinary();

  luaL_rometatable(L, REQUEST_METATABLE, LROT_TABLEREF(httpd_req_mt));

  return 0;
}

NODEMCU_MODULE(HTTPD, "httpd", httpd, lhttpd_init);
