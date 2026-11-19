// Module for WebSocket clients (RFC 6455) on NodeMCU-ESP32.
//
// Wraps the ESP-IDF component "esp_websocket_client" and exposes the same
// Lua API as the ESP8266 NodeMCU websocket module:
//
//   ws = websocket.createClient()
//   ws:config({ headers = { ['User-Agent'] = 'NodeMCU' } })
//   ws:on("connection", function(ws) end)
//   ws:on("receive",    function(ws, msg, opcode) end)
//   ws:on("close",      function(ws, status [, server_code]) end)
//   ws:connect("ws://host:port/path")
//   ws:send(msg [, opcode])
//   ws:close()
//
// Threading model
// ---------------
// esp_websocket_client runs its own FreeRTOS task and delivers events from
// there. Lua must only ever run in the NodeMCU Lua task, so every event is
// copied into a heap-allocated message and posted to the Lua task with
// task_post(). Only the Lua task calls into Lua and only the Lua task
// creates/destroys the esp_websocket_client handle.
//
// Lifetime
// --------
// The per-client state (ws_ctx_t) is NOT stored in the Lua userdata itself,
// because events may still be queued for the Lua task after the userdata has
// been garbage collected. Instead it is malloc'ed and reference counted:
// one reference for the userdata plus one per in-flight event. While a
// connection is active (between connect() and the "close" callback) the
// module keeps a strong registry reference to the userdata so it is not
// collected mid-connection, matching the ESP8266 semantics ("set ws = nil in
// the close callback to let Lua gc the client").

#include "module.h"
#include "lauxlib.h"
#include "lnodeaux.h"
#include "task/task.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_tls_errors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "websocket"
#define WS_METATABLE "websocket.client"

#ifdef CONFIG_NODEMCU_CMODULE_WEBSOCKET_MAX_MSG
#define WS_MAX_MSG CONFIG_NODEMCU_CMODULE_WEBSOCKET_MAX_MSG
#else
#define WS_MAX_MSG 8192
#endif

#define WS_SEND_TIMEOUT_MS   5000   // max time ws:send() may block
#define WS_CLOSE_TIMEOUT_MS  3000   // graceful close handshake before forcing
#define WS_DEFAULT_NET_TIMEOUT_MS 10000

// ESP8266-compatible close status codes (see docs)
#define WS_STATUS_OK                 0
#define WS_STATUS_DNS_FAILED        -5
#define WS_STATUS_SERVER_TERMINATED -6
#define WS_STATUS_BAD_HANDSHAKE     -7
#define WS_STATUS_NO_MEMORY         -8
#define WS_STATUS_NOT_SWITCHING    -17
#define WS_STATUS_CONNECT_TIMEOUT  -18
#define WS_STATUS_NOT_RESPONDING   -19
#define WS_STATUS_UNKNOWN          -99

// WebSocket opcodes
#define WS_OP_CONT   0x0
#define WS_OP_TEXT   0x1
#define WS_OP_BINARY 0x2
#define WS_OP_CLOSE  0x8
#define WS_OP_PING   0x9
#define WS_OP_PONG   0xA

typedef enum {
  EV_CONNECTED,
  EV_DATA,
  EV_CLOSE      // terminal event, posted when the client task has finished
} ws_ev_type_t;

typedef struct {
  esp_websocket_client_handle_t client;

  // Reference count: 1 for the Lua userdata + 1 per queued event.
  volatile int refs;
  bool collected;          // userdata has been garbage collected

  // Lua side (only touched in the Lua task)
  lua_ref_t cb_connection;
  lua_ref_t cb_receive;
  lua_ref_t cb_close;
  lua_ref_t self_ref;      // strong ref while a connection is active
  char *headers;           // "Key: Value\r\n..." or NULL
  char *subprotocol;
  char *ca_cert;
  int ping_interval_sec;
  int network_timeout_ms;

  // Shared: written by the Lua task before close, read by the ws task
  volatile bool user_closing;

  // ws task side (only touched in the websocket task)
  bool was_connected;
  int status;              // close status to report
  int server_code;         // RFC 6455 close code sent by server, 0 if none
  char *rx_buf;
  size_t rx_len;
  size_t rx_cap;
  int rx_opcode;
  bool rx_dropping;        // current message exceeds WS_MAX_MSG
} ws_ctx_t;

typedef struct {
  ws_ctx_t *ctx;
} ws_ud_t;

typedef struct {
  ws_ctx_t *ctx;
  ws_ev_type_t type;
  int opcode;
  int status;
  int server_code;
  size_t len;
  char data[];             // payload for EV_DATA
} ws_event_t;

static task_handle_t ws_task_id = 0;

static const char *const event_names[] = { "connection", "receive", "close", NULL };

// ---------------------------------------------------------------------------
// Context reference counting
// ---------------------------------------------------------------------------

static void ctx_retain(ws_ctx_t *ctx) {
  __atomic_add_fetch(&ctx->refs, 1, __ATOMIC_SEQ_CST);
}

static void ctx_release(ws_ctx_t *ctx) {
  if (__atomic_sub_fetch(&ctx->refs, 1, __ATOMIC_SEQ_CST) == 0) {
    free(ctx->rx_buf);
    free(ctx->headers);
    free(ctx->subprotocol);
    free(ctx->ca_cert);
    free(ctx);
  }
}

// ---------------------------------------------------------------------------
// Websocket task side: convert esp_websocket_client events to task messages
// ---------------------------------------------------------------------------

// Posts an event to the Lua task. 'retries' x 10ms is how long we are willing
// to wait for room in the queue. Never blocks forever, so a Lua task that is
// itself waiting for this client (in ws:close()) cannot deadlock us.
static void post_event(ws_ctx_t *ctx, ws_event_t *ev, int retries) {
  ev->ctx = ctx;
  ctx_retain(ctx);
  for (int i = 0; i <= retries; ++i) {
    if (task_post_medium(ws_task_id, (task_param_t)ev))
      return;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  ESP_LOGW(TAG, "Lua task queue full, dropping event %d", ev->type);
  free(ev);
  ctx_release(ctx);
}

static ws_event_t *new_event(ws_ev_type_t type, size_t len) {
  ws_event_t *ev = calloc(1, sizeof(ws_event_t) + len + 1);
  if (ev) {
    ev->type = type;
    ev->len = len;
  }
  return ev;
}

static void rx_reset(ws_ctx_t *ctx) {
  ctx->rx_len = 0;
  ctx->rx_dropping = false;
}

static bool rx_append(ws_ctx_t *ctx, const char *data, size_t len) {
  if (ctx->rx_dropping)
    return false;
  if (ctx->rx_len + len > WS_MAX_MSG) {
    ESP_LOGW(TAG, "message larger than %d bytes dropped", WS_MAX_MSG);
    ctx->rx_dropping = true;
    return false;
  }
  if (ctx->rx_len + len > ctx->rx_cap) {
    size_t cap = ctx->rx_cap ? ctx->rx_cap : 256;
    while (cap < ctx->rx_len + len)
      cap *= 2;
    if (cap > WS_MAX_MSG)
      cap = WS_MAX_MSG;
    char *nb = realloc(ctx->rx_buf, cap);
    if (!nb) {
      ESP_LOGE(TAG, "out of memory receiving message");
      ctx->rx_dropping = true;
      return false;
    }
    ctx->rx_buf = nb;
    ctx->rx_cap = cap;
  }
  memcpy(ctx->rx_buf + ctx->rx_len, data, len);
  ctx->rx_len += len;
  return true;
}

static void handle_data(ws_ctx_t *ctx, esp_websocket_event_data_t *d) {
  switch (d->op_code) {
    case WS_OP_TEXT:
    case WS_OP_BINARY:
      // A new frame starts when its first chunk arrives (offset 0).
      // Large frames are delivered by the library in several chunks with the
      // same opcode and increasing payload_offset.
      if (d->payload_offset == 0) {
        rx_reset(ctx);
        ctx->rx_opcode = d->op_code;
      }
      break;
    case WS_OP_CONT:
      break;
    case WS_OP_CLOSE:
      // Close frame from the server: first two bytes are the status code
      if (d->payload_offset == 0 && d->data_len >= 2) {
        const uint8_t *p = (const uint8_t *)d->data_ptr;
        ctx->server_code = (p[0] << 8) | p[1];
      }
      return;
    default:       // ping / pong are handled by the library
      return;
  }

  if (d->data_len > 0)
    rx_append(ctx, d->data_ptr, d->data_len);

  bool frame_complete = (d->payload_offset + d->data_len >= d->payload_len);
  if (!(frame_complete && d->fin))
    return;        // wait for more chunks / continuation frames

  if (!ctx->rx_dropping) {
    ws_event_t *ev = new_event(EV_DATA, ctx->rx_len);
    if (ev) {
      ev->opcode = ctx->rx_opcode;
      if (ctx->rx_len)
        memcpy(ev->data, ctx->rx_buf, ctx->rx_len);
      post_event(ctx, ev, 20);
    } else {
      ESP_LOGE(TAG, "out of memory delivering message");
    }
  }
  rx_reset(ctx);
}

static int map_disconnect_status(ws_ctx_t *ctx, esp_websocket_event_data_t *d) {
  if (ctx->user_closing)
    return WS_STATUS_OK;
  switch (d->error_handle.error_type) {
    case WEBSOCKET_ERROR_TYPE_HANDSHAKE:
      return d->error_handle.esp_ws_handshake_status_code > 0
        ? WS_STATUS_NOT_SWITCHING : WS_STATUS_BAD_HANDSHAKE;
    case WEBSOCKET_ERROR_TYPE_PONG_TIMEOUT:
      return WS_STATUS_NOT_RESPONDING;
    case WEBSOCKET_ERROR_TYPE_SERVER_CLOSE:
      return WS_STATUS_SERVER_TERMINATED;
    case WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT:
      if (!ctx->was_connected) {
        if (d->error_handle.esp_tls_last_esp_err == ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME)
          return WS_STATUS_DNS_FAILED;
        return WS_STATUS_CONNECT_TIMEOUT;
      }
      return WS_STATUS_NOT_RESPONDING;
    default:
      return WS_STATUS_UNKNOWN;
  }
}

static void ws_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data) {
  (void)base;
  ws_ctx_t *ctx = (ws_ctx_t *)arg;
  esp_websocket_event_data_t *d = (esp_websocket_event_data_t *)event_data;

  switch (id) {
    case WEBSOCKET_EVENT_CONNECTED: {
      ctx->was_connected = true;
      ctx->status = WS_STATUS_OK;
      ctx->server_code = 0;
      rx_reset(ctx);
      ws_event_t *ev = new_event(EV_CONNECTED, 0);
      if (ev)
        post_event(ctx, ev, 100);
      break;
    }
    case WEBSOCKET_EVENT_DATA:
      handle_data(ctx, d);
      break;
    case WEBSOCKET_EVENT_DISCONNECTED:
      ctx->status = map_disconnect_status(ctx, d);
      break;
    case WEBSOCKET_EVENT_CLOSED:
      ctx->status = WS_STATUS_OK;   // clean close handshake
      break;
    case WEBSOCKET_EVENT_FINISH: {
      // Last event before the client task exits. Always delivered.
      ws_event_t *ev = new_event(EV_CLOSE, 0);
      if (ev) {
        ev->status = ctx->user_closing ? WS_STATUS_OK : ctx->status;
        ev->server_code = ctx->server_code;
        post_event(ctx, ev, 500);   // up to 5s
      } else {
        ESP_LOGE(TAG, "out of memory posting close event");
      }
      break;
    }
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Lua task side
// ---------------------------------------------------------------------------

static void call_cb(lua_State *L, lua_ref_t cb, int nargs) {
  // stack: obj, arg1..argN  -> calls cb(obj, arg1..argN)
  lua_rawgeti(L, LUA_REGISTRYINDEX, cb);
  lua_insert(L, -(nargs + 2));
  if (luaL_pcallx(L, nargs + 1, 0) != 0) {
    // luaL_pcallx already reported the error via the NodeMCU error handler
    lua_pop(L, 1);
  }
}

static void destroy_client(ws_ctx_t *ctx) {
  if (ctx->client) {
    // Safe from the Lua task: waits for the client task to stop if needed.
    esp_websocket_client_destroy(ctx->client);
    ctx->client = NULL;
  }
}

static void ws_task_handler(task_param_t param, task_prio_t prio) {
  (void)prio;
  ws_event_t *ev = (ws_event_t *)param;
  ws_ctx_t *ctx = ev->ctx;
  lua_State *L = lua_getstate();

  if (ctx->collected || L == NULL || !luaX_valid_ref(ctx->self_ref))
    goto done;

  int top = lua_gettop(L);
  lua_checkstack(L, 6);
  // Push the client object; this also pins it during the callback.
  lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->self_ref);

  switch (ev->type) {
    case EV_CONNECTED:
      if (luaX_valid_ref(ctx->cb_connection)) {
        lua_pushvalue(L, -1);
        call_cb(L, ctx->cb_connection, 0);
      }
      break;

    case EV_DATA:
      if (luaX_valid_ref(ctx->cb_receive)) {
        lua_pushvalue(L, -1);
        lua_pushlstring(L, ev->data, ev->len);
        lua_pushinteger(L, ev->opcode);
        call_cb(L, ctx->cb_receive, 2);
      }
      break;

    case EV_CLOSE:
      // The connection is over: release the client handle and the strong
      // reference *before* calling the user, so the callback may call
      // ws:connect() again or drop its last reference (ws = nil).
      destroy_client(ctx);
      ctx->user_closing = false;
      luaX_unset_ref(L, &ctx->self_ref);   // object stays pinned on the stack
      if (luaX_valid_ref(ctx->cb_close)) {
        lua_pushvalue(L, -1);
        lua_pushinteger(L, ev->status);
        if (ev->server_code) {
          lua_pushinteger(L, ev->server_code);
          call_cb(L, ctx->cb_close, 2);
        } else {
          call_cb(L, ctx->cb_close, 1);
        }
      }
      break;
  }

  lua_settop(L, top);

done:
  if (ev->type == EV_CLOSE && ctx->collected)
    destroy_client(ctx);
  free(ev);
  ctx_release(ctx);
}

static ws_ctx_t *check_ctx(lua_State *L) {
  ws_ud_t *ud = (ws_ud_t *)luaL_checkudata(L, 1, WS_METATABLE);
  return ud->ctx;
}

// Lua: websocket.createClient()
static int ws_create_client(lua_State *L) {
  ws_ctx_t *ctx = calloc(1, sizeof(ws_ctx_t));
  if (!ctx)
    return luaL_error(L, "out of memory");
  ctx->refs = 1;
  ctx->cb_connection = ctx->cb_receive = ctx->cb_close = LUA_NOREF;
  ctx->self_ref = LUA_NOREF;
  ctx->network_timeout_ms = WS_DEFAULT_NET_TIMEOUT_MS;

  ws_ud_t *ud = (ws_ud_t *)lua_newuserdata(L, sizeof(ws_ud_t));
  ud->ctx = ctx;
  luaL_getmetatable(L, WS_METATABLE);
  lua_setmetatable(L, -2);

  if (ws_task_id == 0)
    ws_task_id = task_get_id(ws_task_handler);
  return 1;
}

static char *dup_string(lua_State *L, int idx) {
  size_t len;
  const char *s = lua_tolstring(L, idx, &len);
  char *r = malloc(len + 1);
  if (!r)
    luaL_error(L, "out of memory");
  memcpy(r, s, len);
  r[len] = '\0';
  return r;
}

// Lua: ws:config({ headers = {...}, subprotocol = "...", ping_interval = s,
//                  timeout = ms, ca_cert = "PEM" })
static int ws_config(lua_State *L) {
  ws_ctx_t *ctx = check_ctx(L);
  luaL_checktype(L, 2, LUA_TTABLE);

  lua_getfield(L, 2, "headers");
  if (lua_istable(L, -1)) {
    // Build "Key: Value\r\n..." in C (luaL_Buffer would disturb the stack
    // indices that lua_next relies on).
    int t = lua_gettop(L);
    size_t total = 0;
    lua_pushnil(L);
    while (lua_next(L, t) != 0) {
      if (lua_type(L, -2) == LUA_TSTRING && lua_isstring(L, -1)) {
        size_t kl, vl;
        lua_tolstring(L, -2, &kl);
        lua_tolstring(L, -1, &vl);   // converts numbers in the value slot only
        total += kl + vl + 4;
      }
      lua_pop(L, 1);
    }
    char *h = NULL;
    if (total > 0) {
      h = malloc(total + 1);
      if (!h)
        return luaL_error(L, "out of memory");
      char *p = h;
      lua_pushnil(L);
      while (lua_next(L, t) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING && lua_isstring(L, -1)) {
          size_t kl, vl;
          const char *k = lua_tolstring(L, -2, &kl);
          const char *v = lua_tolstring(L, -1, &vl);
          memcpy(p, k, kl); p += kl;
          memcpy(p, ": ", 2); p += 2;
          memcpy(p, v, vl); p += vl;
          memcpy(p, "\r\n", 2); p += 2;
        }
        lua_pop(L, 1);
      }
      *p = '\0';
    }
    free(ctx->headers);
    ctx->headers = h;
  }
  lua_pop(L, 1);

  lua_getfield(L, 2, "subprotocol");
  if (lua_isstring(L, -1)) {
    free(ctx->subprotocol);
    ctx->subprotocol = dup_string(L, -1);
  }
  lua_pop(L, 1);

  lua_getfield(L, 2, "ca_cert");
  if (lua_isstring(L, -1)) {
    free(ctx->ca_cert);
    ctx->ca_cert = dup_string(L, -1);
  }
  lua_pop(L, 1);

  lua_getfield(L, 2, "ping_interval");
  if (lua_isnumber(L, -1))
    ctx->ping_interval_sec = lua_tointeger(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 2, "timeout");
  if (lua_isnumber(L, -1))
    ctx->network_timeout_ms = lua_tointeger(L, -1);
  lua_pop(L, 1);

  return 0;
}

// Lua: ws:on(event, function | nil)
static int ws_on(lua_State *L) {
  ws_ctx_t *ctx = check_ctx(L);
  int ev = luaL_checkoption(L, 2, NULL, event_names);
  lua_ref_t *slot = ev == 0 ? &ctx->cb_connection
                  : ev == 1 ? &ctx->cb_receive
                            : &ctx->cb_close;
  if (lua_isfunction(L, 3))
    luaX_set_ref(L, 3, slot);
  else if (lua_isnoneornil(L, 3))
    luaX_unset_ref(L, slot);
  else
    return luaL_argerror(L, 3, "function or nil expected");
  return 0;
}

// Lua: ws:connect(url)
static int ws_connect(lua_State *L) {
  ws_ctx_t *ctx = check_ctx(L);
  const char *url = luaL_checkstring(L, 2);

  if (ctx->client)
    return luaL_error(L, "already connected or connecting; call close() first");
  if (strncmp(url, "ws://", 5) != 0 && strncmp(url, "wss://", 6) != 0)
    return luaL_error(L, "url must start with ws:// or wss://");

  esp_websocket_client_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.uri = url;                        // copied by esp_websocket_client_init
  cfg.headers = ctx->headers;
  cfg.subprotocol = ctx->subprotocol;
  cfg.cert_pem = ctx->ca_cert;
  cfg.disable_auto_reconnect = true;    // ESP8266 semantics: close, then app decides
  cfg.network_timeout_ms = ctx->network_timeout_ms;
  if (ctx->ping_interval_sec > 0)
    cfg.ping_interval_sec = ctx->ping_interval_sec;

  ctx->client = esp_websocket_client_init(&cfg);
  if (!ctx->client)
    return luaL_error(L, "failed to create websocket client");

  esp_websocket_register_events(ctx->client, WEBSOCKET_EVENT_ANY, ws_event_handler, ctx);

  ctx->user_closing = false;
  ctx->was_connected = false;
  ctx->status = WS_STATUS_UNKNOWN;
  ctx->server_code = 0;

  // keep the object alive until the "close" event has been delivered
  luaX_set_ref(L, 1, &ctx->self_ref);

  if (esp_websocket_client_start(ctx->client) != ESP_OK) {
    esp_websocket_client_destroy(ctx->client);
    ctx->client = NULL;
    luaX_unset_ref(L, &ctx->self_ref);
    return luaL_error(L, "failed to start websocket client");
  }
  return 0;
}

// Lua: ws:send(message [, opcode])
static int ws_send(lua_State *L) {
  ws_ctx_t *ctx = check_ctx(L);
  size_t len;
  const char *msg = luaL_checklstring(L, 2, &len);
  int opcode = luaL_optinteger(L, 3, WS_OP_TEXT);

  if (opcode != WS_OP_TEXT && opcode != WS_OP_BINARY && opcode != WS_OP_PING)
    return luaL_argerror(L, 3, "opcode must be 1 (text), 2 (binary) or 9 (ping)");
  if (!ctx->client || !esp_websocket_client_is_connected(ctx->client))
    return luaL_error(L, "websocket not connected");

  int sent = esp_websocket_client_send_with_opcode(ctx->client,
      (ws_transport_opcodes_t)opcode, (const uint8_t *)msg, (int)len,
      pdMS_TO_TICKS(WS_SEND_TIMEOUT_MS));
  if (sent < 0)
    return luaL_error(L, "websocket send failed");
  return 0;
}

// Lua: ws:close()
// Initiates a graceful close. The "close" callback follows asynchronously.
static int ws_close(lua_State *L) {
  ws_ctx_t *ctx = check_ctx(L);
  if (!ctx->client)
    return 0;   // nothing to do, as on the ESP8266
  ctx->user_closing = true;
  if (esp_websocket_client_is_connected(ctx->client)) {
    // Sends a close frame, waits for the handshake (or forces after timeout)
    esp_websocket_client_close(ctx->client, pdMS_TO_TICKS(WS_CLOSE_TIMEOUT_MS));
  } else {
    // still connecting (or already shutting down): just stop the task
    esp_websocket_client_stop(ctx->client);
  }
  // The client task has ended now; its FINISH event is queued and will
  // destroy the handle and fire the "close" callback.
  return 0;
}

// __gc
static int ws_gc(lua_State *L) {
  ws_ud_t *ud = (ws_ud_t *)luaL_checkudata(L, 1, WS_METATABLE);
  ws_ctx_t *ctx = ud->ctx;
  if (!ctx)
    return 0;
  ud->ctx = NULL;
  ctx->collected = true;
  ctx->user_closing = true;
  destroy_client(ctx);   // normally already NULL (strong ref held while active)
  luaX_unset_ref(L, &ctx->cb_connection);
  luaX_unset_ref(L, &ctx->cb_receive);
  luaX_unset_ref(L, &ctx->cb_close);
  luaX_unset_ref(L, &ctx->self_ref);
  ctx_release(ctx);      // pending events hold their own references
  return 0;
}

LROT_BEGIN(websocket_client, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY(__gc, ws_gc)
  LROT_TABENTRY(__index, websocket_client)
  LROT_FUNCENTRY(config, ws_config)
  LROT_FUNCENTRY(connect, ws_connect)
  LROT_FUNCENTRY(send, ws_send)
  LROT_FUNCENTRY(close, ws_close)
  LROT_FUNCENTRY(on, ws_on)
LROT_END(websocket_client, NULL, LROT_MASK_GC_INDEX)

LROT_BEGIN(websocket, NULL, 0)
  LROT_FUNCENTRY(createClient, ws_create_client)
  LROT_NUMENTRY(TEXT, WS_OP_TEXT)
  LROT_NUMENTRY(BINARY, WS_OP_BINARY)
LROT_END(websocket, NULL, 0)

static int luaopen_websocket(lua_State *L) {
  luaL_rometatable(L, WS_METATABLE, LROT_TABLEREF(websocket_client));
  return 0;
}

NODEMCU_MODULE(WEBSOCKET, "websocket", websocket, luaopen_websocket);
