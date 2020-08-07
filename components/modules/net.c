// Module for network

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "lmem.h"
#include "ip_fmt.h"
#include "task/task.h"

#include <string.h>
#include <stdlib.h>
#include <string.h>

#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/dns.h"
#include "lwip/tcp.h"
#include "lwip/inet.h"
#include "lwip/igmp.h"

#include <esp_log.h>

// Some LWIP macros cause complaints with ptr NULL checks, so shut them off :(
#pragma GCC diagnostic ignored "-Waddress"

typedef enum net_type {
  TYPE_TCP_SERVER = 0,
  TYPE_TCP_CLIENT,
  TYPE_UDP_SOCKET
} net_type;

static const char *NET_TABLES[] = {
  "net.tcpserver",
  "net.tcpsocket",
  "net.udpsocket"
};
#define NET_TABLE_TCP_SERVER NET_TABLES[0]
#define NET_TABLE_TCP_CLIENT NET_TABLES[1]
#define NET_TABLE_UDP_SOCKET NET_TABLES[2]

#define TYPE_TCP TYPE_TCP_CLIENT
#define TYPE_UDP TYPE_UDP_SOCKET


static task_handle_t net_handler;
static task_handle_t dns_handler;

typedef enum {
  EVT_SENT, EVT_RECV, EVT_ZEROREAD, EVT_CONNECTED, EVT_ERR
} bounce_event_t;

typedef struct netconn_bounce_event {
  struct netconn *netconn;
  bounce_event_t event;
  err_t err;
  uint16_t len;
} netconn_bounce_event_t;


struct lnet_userdata;

typedef struct dns_event {
  ip_addr_t resolved_ip;
  enum { DNS_STATIC, DNS_SOCKET } invoker;
  union {
    int cb_ref;
    struct lnet_userdata *ud;
  };
} dns_event_t;


// --- lwIP callbacks -----------------------------------------------------

// Caution - these run in a different RTOS thread!

static void lnet_netconn_callback(struct netconn *netconn, enum netconn_evt evt, u16_t len)
{
  if (!netconn) return;

  switch (evt)
  {
    case NETCONN_EVT_SENDPLUS:
    case NETCONN_EVT_RCVPLUS:
    case NETCONN_EVT_ERROR: break;
    default: return; // we don't care about minus events
  }

  netconn_bounce_event_t *nbe = malloc(sizeof(netconn_bounce_event_t));
  if (!nbe)
  {
    ESP_LOGE("net", "out of memory - lost event notification!");
    return;
  }

  nbe->netconn = netconn;
  nbe->len = len;
  nbe->err = ERR_OK;
  task_prio_t prio = TASK_PRIORITY_MEDIUM;
  switch (evt)
  {
    case NETCONN_EVT_SENDPLUS:
      nbe->event = (len == 0) ? EVT_CONNECTED : EVT_SENT; break;
    case NETCONN_EVT_RCVPLUS:
      nbe->event = (len == 0) ? EVT_ZEROREAD : EVT_RECV;
      prio = TASK_PRIORITY_HIGH;
      break;
    case NETCONN_EVT_ERROR:
      nbe->event = EVT_ERR;
      nbe->err = netconn_err(netconn); // is this safe in this thread?
      break;
    default: break; // impossible
  }
  if (!task_post(prio, net_handler, (task_param_t)nbe))
    ESP_LOGE("net", "out of task post slots - lost data!");
}


// This one may *also* run in the LVM thread
static void net_dns_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
  dns_event_t *ev = (dns_event_t *)callback_arg;

  ev->resolved_ip = ipaddr ? *ipaddr : ip_addr_any;

  if (!task_post_medium(dns_handler, (task_param_t)ev))
  {
    ESP_LOGE("net", "out of task post slots - lost DNS response for %s", name);
    // And we also leaked the ev here :(
  }
}


// --- active list (out of reach of the lwIP thread) ---------------------

typedef struct lnet_userdata {
  enum net_type type;
  int self_ref;
  uint16_t port;
  struct netconn *netconn;
  bool closing;
  union {
    struct {
      int cb_accept_ref;
      int timeout;
    } server;
    struct {
      int wait_dns;
      int cb_dns_ref;
      int cb_receive_ref;
      int cb_sent_ref;
      // Only for TCP:
      bool connecting;
      int hold;
      size_t num_held;
      size_t num_send;
      int cb_connect_ref;
      int cb_disconnect_ref;
      int cb_reconnect_ref;
    } client;
  };

  struct lnet_userdata *next;
} lnet_userdata;

static lnet_userdata *active;


static void add_userdata_to_active(lnet_userdata *ud)
{
  ud->next = active;
  active = ud;
}


static void remove_userdata_from_active(lnet_userdata *ud)
{
  if (!ud)
    return;
  for (lnet_userdata **i = &active; *i; i = &(*i)->next)
  {
    if (*i == ud)
    {
      *i = ud->next;
      return;
    }
  }
}


static lnet_userdata *userdata_from_netconn(struct netconn *nc)
{
  for (lnet_userdata *i = active; i; i = i->next)
    if (i->netconn == nc)
      return i;
  return NULL;
}


// --- Forward declarations ----------------------------------------------

static void lsent_cb (lua_State *L, lnet_userdata *ud);
static void lrecv_cb (lua_State *L, lnet_userdata *ud);
static void laccept_cb (lua_State *L, lnet_userdata *ud);
static void lconnected_cb (lua_State *L, lnet_userdata *ud);
static void lerr_cb (lua_State *L, lnet_userdata *ud, err_t err);
static void ldnsstatic_cb (lua_State *L, int cb_ref, ip_addr_t *addr);
static void ldnsfound_cb (lua_State *L, lnet_userdata *ud, ip_addr_t *addr);


// --- Helper functions --------------------------------------------------

int lwip_lua_checkerr (lua_State *L, err_t err) {
  switch (err) {
    case ERR_OK: return 0;
    case ERR_MEM: return luaL_error(L, "out of memory");
    case ERR_BUF: return luaL_error(L, "buffer error");
    case ERR_TIMEOUT: return luaL_error(L, "timeout");
    case ERR_RTE: return luaL_error(L, "routing problem");
    case ERR_INPROGRESS: return luaL_error(L, "in progress");
    case ERR_VAL: return luaL_error(L, "illegal value");
    case ERR_WOULDBLOCK: return luaL_error(L, "would block");
    case ERR_ABRT: return luaL_error(L, "connection aborted");
    case ERR_RST: return luaL_error(L, "connection reset");
    case ERR_CLSD: return luaL_error(L, "connection closed");
    case ERR_CONN: return luaL_error(L, "not connected");
    case ERR_ARG: return luaL_error(L, "illegal argument");
    case ERR_USE: return luaL_error(L, "address in use");
    case ERR_IF: return luaL_error(L, "netif error");
    case ERR_ISCONN: return luaL_error(L, "already connected");
    default: return luaL_error(L, "unknown error");
  }
}


lnet_userdata *net_create( lua_State *L, enum net_type type ) {
  const char *mt = NET_TABLES[type];
  lnet_userdata *ud = (lnet_userdata *)lua_newuserdata(L, sizeof(lnet_userdata));
  if (!ud) return NULL;

  luaL_getmetatable(L, mt);
  lua_setmetatable(L, -2);

  ud->type = type;
  ud->self_ref = LUA_NOREF;
  ud->netconn = NULL;
  ud->closing = false;

  switch (type) {
    case TYPE_TCP_CLIENT:
      ud->client.cb_connect_ref = LUA_NOREF;
      ud->client.cb_reconnect_ref = LUA_NOREF;
      ud->client.cb_disconnect_ref = LUA_NOREF;
      ud->client.hold = 0;
      ud->client.num_held = 0;
      ud->client.connecting = false;
    case TYPE_UDP_SOCKET:
      ud->client.wait_dns = 0;
      ud->client.cb_dns_ref = LUA_NOREF;
      ud->client.cb_receive_ref = LUA_NOREF;
      ud->client.cb_sent_ref = LUA_NOREF;
      break;
    case TYPE_TCP_SERVER:
      ud->server.cb_accept_ref = LUA_NOREF;
      break;
  }

  add_userdata_to_active(ud);

  return ud;
}


// --- LVM thread task handlers -----------------------------------------

void handle_net_event(task_param_t param, task_prio_t prio)
{
  (void)prio;
  netconn_bounce_event_t *pnbe = (netconn_bounce_event_t *)param;
  netconn_bounce_event_t nbe = *pnbe;
  free(pnbe); // free before we can hit any luaL_error()s

  lnet_userdata *ud = userdata_from_netconn(nbe.netconn);
  if (!ud)
    return;

  if (nbe.event == EVT_ERR)
  {
    lerr_cb(lua_getstate(), ud, nbe.err);
    return;
  }

  if (ud->type == TYPE_TCP_CLIENT || ud->type == TYPE_UDP_SOCKET)
  {
    switch (nbe.event)
    {
      case EVT_CONNECTED:
        if (ud->type == TYPE_TCP_CLIENT && ud->client.connecting)
        {
          ud->client.connecting = false;
          lconnected_cb(lua_getstate(), ud);
        }
        break;
      case EVT_SENT:
        ud->client.num_send -= nbe.len;
        if (ud->client.num_send == 0) // all data sent, trigger Lua callback
          lsent_cb(lua_getstate(), ud);
        break;
      case EVT_RECV:
        if (nbe.len > 0)
          lrecv_cb(lua_getstate(), ud);
        break;
      case EVT_ZEROREAD: // fall-through
      case EVT_ERR:
        ud->closing = true;
        lerr_cb(lua_getstate(), ud, nbe.err);
        break;
    }
  }
  else if (ud->type == TYPE_TCP_SERVER)
  {
    switch (nbe.event)
    {
      case EVT_ZEROREAD: // new connection
        laccept_cb (lua_getstate(), ud);
        break;
      case EVT_ERR:
        ud->closing = true;
        lerr_cb(lua_getstate(), ud, nbe.err);
        break;
      default: break;
    }
  }
}


void handle_dns_event(task_param_t param, task_prio_t prio)
{
  (void)prio;
  dns_event_t *pev = (dns_event_t *)param;
  dns_event_t ev = *pev;

  lua_State *L = lua_getstate();
  luaM_free(L, pev); // free before we can hit any luaL_error()s

  switch (ev.invoker)
  {
    case DNS_STATIC:
      ldnsstatic_cb(L, ev.cb_ref, &ev.resolved_ip);
      break;
    case DNS_SOCKET:
      ldnsfound_cb(L, ev.ud, &ev.resolved_ip);
  }
}


static err_t netconn_close_and_delete(struct netconn *conn) {
  err_t err = netconn_close(conn);
  if (err == ERR_OK)
    netconn_delete(conn);

  return err;
}


// --- Lua API - create

// Lua: net.createUDPSocket()
static int net_createUDPSocket( lua_State *L ) {
  net_create(L, TYPE_UDP_SOCKET);
  return 1;
}

// Lua: net.createServer(type, timeout)
static int net_createServer( lua_State *L ) {
  int type, timeout;

  type = luaL_optlong(L, 1, TYPE_TCP);
  timeout = luaL_optlong(L, 2, 30);

  if (type == TYPE_UDP) return net_createUDPSocket( L );
  if (type != TYPE_TCP) return luaL_error(L, "invalid type");

  lnet_userdata *ud = net_create(L, TYPE_TCP_SERVER);
  ud->server.timeout = timeout;
  return 1;
}

// Lua: net.createConnection(type, secure)
static int net_createConnection( lua_State *L ) {
  int type, secure;

  type = luaL_optlong(L, 1, TYPE_TCP);
  secure = luaL_optlong(L, 2, 0);

  if (type == TYPE_UDP) return net_createUDPSocket( L );
  if (type != TYPE_TCP) return luaL_error(L, "invalid type");
  if (secure) {
    return luaL_error(L, "secure connections not yet supported");
  }
  net_create(L, TYPE_TCP_CLIENT);
  return 1;
}

// --- Get & check userdata

static lnet_userdata *net_get_udata_s( lua_State *L, int stack ) {
  if (!lua_isuserdata(L, stack)) return NULL;
  lnet_userdata *ud = (lnet_userdata *)lua_touserdata(L, stack);
  switch (ud->type) {
    case TYPE_TCP_CLIENT:
    case TYPE_TCP_SERVER:
    case TYPE_UDP_SOCKET:
      break;
    default: return NULL;
  }
  const char *mt = NET_TABLES[ud->type];
  ud = luaL_checkudata(L, stack, mt);
  return ud;
}
#define net_get_udata(L) net_get_udata_s(L, 1)


static int lnet_socket_resolve_dns(lua_State *L, lnet_userdata *ud, const char * domain, bool close_on_err)
{
  ud->client.wait_dns ++;
  int unref = 0;
  if (ud->self_ref == LUA_NOREF) {
    unref = 1;
    lua_pushvalue(L, 1);
    ud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  dns_event_t *ev = luaM_new(L, dns_event_t);
  if (!ev)
    return luaL_error (L, "out of memory");

  ev->invoker = DNS_SOCKET;
  ev->ud = ud;

  err_t err = dns_gethostbyname(domain, &ev->resolved_ip, net_dns_cb, ev);
  if (err == ERR_OK)
    net_dns_cb(domain, &ev->resolved_ip, ev);
  else if (err != ERR_INPROGRESS)
  {
    luaM_free(L, ev);
    ud->client.wait_dns --;
    if (unref) {
      luaL_unref(L, LUA_REGISTRYINDEX, ud->self_ref);
      ud->self_ref = LUA_NOREF;
    }
    if (close_on_err)
    {
      ud->closing = true;
      if (netconn_close_and_delete(ud->netconn) == ERR_OK)
        ud->netconn = NULL;
    }
    return lwip_lua_checkerr(L, err);
  }
  return 0;
}


// --- Lua API

// Lua: server:listen(port, addr, function(c)), socket:listen(port, addr)
static int net_listen( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type == TYPE_TCP_CLIENT)
    return luaL_error(L, "invalid user data");
  if (ud->netconn)
    return luaL_error(L, "already listening");
  int stack = 2;
  uint16_t port = 0;
  const char *domain = "0.0.0.0";
  if (lua_isnumber(L, stack))
    port = lua_tointeger(L, stack++);
  if (lua_isstring(L, stack)) {
    size_t dl = 0;
    domain = luaL_checklstring(L, stack++, &dl);
  }
  ip_addr_t addr;
  if (!ipaddr_aton(domain, &addr))
    return luaL_error(L, "invalid IP address");
  if (ud->type == TYPE_TCP_SERVER) {
    if (lua_isfunction(L, stack) || lua_islightfunction(L, stack)) {
      lua_pushvalue(L, stack++);
      luaL_unref(L, LUA_REGISTRYINDEX, ud->server.cb_accept_ref);
      ud->server.cb_accept_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
      return luaL_error(L, "need callback");
    }
  }
  err_t err = ERR_OK;
  switch (ud->type) {
    case TYPE_TCP_SERVER:
      ud->netconn = netconn_new_with_callback(NETCONN_TCP, lnet_netconn_callback);
      if (!ud->netconn)
        return luaL_error(L, "cannot allocate netconn");
      netconn_set_nonblocking(ud->netconn, 1);
      netconn_set_noautorecved(ud->netconn, 1);

      err = netconn_bind(ud->netconn, &addr, port);
      if (err == ERR_OK) {
        err = netconn_listen(ud->netconn);
      }
      break;
    case TYPE_UDP_SOCKET:
      ud->netconn = netconn_new_with_callback(NETCONN_UDP, lnet_netconn_callback);
      if (!ud->netconn)
        return luaL_error(L, "cannot allocate netconn");
      netconn_set_nonblocking(ud->netconn, 1);
      netconn_set_noautorecved(ud->netconn, 1);

      err = netconn_bind(ud->netconn, &addr, port);
      break;
    default: break;
  }
  if (err != ERR_OK) {
    ud->closing = true;
    switch (ud->type) {
      case TYPE_TCP_SERVER:
        if (netconn_close_and_delete(ud->netconn) == ERR_OK)
          ud->netconn = NULL;
        break;
      case TYPE_UDP_SOCKET:
        if (netconn_close_and_delete(ud->netconn) == ERR_OK)
          ud->netconn = NULL;
        break;
      default: break;
    }
    return lwip_lua_checkerr(L, err);
  }
  if (ud->self_ref == LUA_NOREF) {
    lua_pushvalue(L, 1);
    ud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  return 0;
}

// Lua: client:connect(port, addr)
static int net_connect( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type != TYPE_TCP_CLIENT)
    return luaL_error(L, "invalid user data");
  if (ud->netconn)
    return luaL_error(L, "already connected");
  uint16_t port = luaL_checkinteger(L, 2);
  if (port == 0)
    return luaL_error(L, "specify port");
  const char *domain = "127.0.0.1";
  if (lua_isstring(L, 3)) {
    size_t dl = 0;
    domain = luaL_checklstring(L, 3, &dl);
  }

  ud->netconn = netconn_new_with_callback(NETCONN_TCP, lnet_netconn_callback);
  if (!ud->netconn)
    return luaL_error(L, "cannot allocate netconn");
  netconn_set_nonblocking(ud->netconn, 1);
  netconn_set_noautorecved(ud->netconn, 1);
  ud->port = port;

  return lnet_socket_resolve_dns(L, ud, domain, true);
}

// Lua: client/socket:on(name, callback)
static int net_on( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type == TYPE_TCP_SERVER)
    return luaL_error(L, "invalid user data");
  int *refptr = NULL;
  const char *name = luaL_checkstring(L, 2);
  if (!name) return luaL_error(L, "need callback name");
  switch (ud->type) {
    case TYPE_TCP_CLIENT:
      if (strcmp("connection",name)==0)
        { refptr = &ud->client.cb_connect_ref; break; }
      if (strcmp("disconnection",name)==0)
        { refptr = &ud->client.cb_disconnect_ref; break; }
      if (strcmp("reconnection",name)==0)
        { refptr = &ud->client.cb_reconnect_ref; break; }
    case TYPE_UDP_SOCKET:
      if (strcmp("dns",name)==0)
        { refptr = &ud->client.cb_dns_ref; break; }
      if (strcmp("receive",name)==0)
        { refptr = &ud->client.cb_receive_ref; break; }
      if (strcmp("sent",name)==0)
        { refptr = &ud->client.cb_sent_ref; break; }
      break;
    default: return luaL_error(L, "invalid user data");
  }
  if (refptr == NULL)
    return luaL_error(L, "invalid callback name");
  if (lua_isfunction(L, 3) || lua_islightfunction(L, 3)) {
    lua_pushvalue(L, 3);
    luaL_unref(L, LUA_REGISTRYINDEX, *refptr);
    *refptr = luaL_ref(L, LUA_REGISTRYINDEX);
  } else if (lua_isnil(L, 3)) {
    luaL_unref(L, LUA_REGISTRYINDEX, *refptr);
    *refptr = LUA_NOREF;
  } else {
    return luaL_error(L, "invalid callback function");
  }
  return 0;
}

// Lua: client:send(data, function(c)), socket:send(port, ip, data, function(s))
static int net_send( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type == TYPE_TCP_SERVER)
    return luaL_error(L, "invalid user data");
  ip_addr_t addr;
  uint16_t port = 0;
  const char *data;
  size_t datalen = 0;
  int stack = 2;
  if (ud->type == TYPE_UDP_SOCKET) {
    size_t dl = 0;
    port = luaL_checkinteger(L, stack++);
    if (port == 0) return luaL_error(L, "need port");
    const char *domain = luaL_checklstring(L, stack++, &dl);
    if (!domain) return luaL_error(L, "need IP address");
    if (!ipaddr_aton(domain, &addr)) return luaL_error(L, "invalid IP address");
  }
  data = luaL_checklstring(L, stack++, &datalen);
  if (!data || datalen == 0) return luaL_error(L, "no data to send");
  ud->client.num_send = datalen;
  if (lua_isfunction(L, stack) || lua_islightfunction(L, stack)) {
    lua_pushvalue(L, stack++);
    luaL_unref(L, LUA_REGISTRYINDEX, ud->client.cb_sent_ref);
    ud->client.cb_sent_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  if (ud->type == TYPE_UDP_SOCKET && !ud->netconn) {
    ud->netconn = netconn_new_with_callback(NETCONN_UDP, lnet_netconn_callback);
    if (!ud->netconn)
      return luaL_error(L, "cannot allocate netconn");
    err_t err = netconn_bind(ud->netconn, IP_ADDR_ANY, 0);
    if (err != ERR_OK) {
      ud->closing = true;
      if (netconn_close_and_delete(ud->netconn) == ERR_OK)
        ud->netconn = NULL;
      return lwip_lua_checkerr(L, err);
    }
    if (ud->self_ref == LUA_NOREF) {
      lua_pushvalue(L, 1);
      ud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
  }

  if (!ud->netconn || ud->self_ref == LUA_NOREF)
    return luaL_error(L, "not connected");

  err_t err;
  if (ud->type == TYPE_UDP_SOCKET) {
    struct netbuf *buf = netbuf_new();
    if (!buf || !netbuf_alloc(buf, datalen))
      return luaL_error(L, "cannot allocate message buffer");
    netbuf_take(buf, data, datalen);
    err = netconn_sendto(ud->netconn, buf, &addr, port);
    netbuf_delete(buf);

    if (ud->client.cb_sent_ref != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, ud->client.cb_sent_ref);
      lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
      lua_call(L, 1, 0);
    }
  } else if (ud->type == TYPE_TCP_CLIENT) {
    size_t bytes_written;
    err = netconn_write_partly(ud->netconn, data, datalen, NETCONN_COPY, &bytes_written);
    if (err == ERR_OK && (datalen != bytes_written)) {
      // the string object is potentially gc'ed after net_send finishes and we can't ensure
      // integrity of the data pointer to netconn -> signal error to Lua layer
      err = ERR_BUF;
    }
  }
  else {
    err = ERR_VAL;
  }
  return lwip_lua_checkerr(L, err);
}

// Lua: client:hold()
static int net_hold( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type != TYPE_TCP_CLIENT)
    return luaL_error(L, "invalid user data");
  if (!ud->client.hold && ud->netconn)
  {
    if (ud->client.hold == 0)
    {
      ud->client.hold = 1;
      ud->client.num_held = 0;
    }
  }
  return 0;
}

// Lua: client:unhold()
static int net_unhold( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type != TYPE_TCP_CLIENT)
    return luaL_error(L, "invalid user data");
  if (ud->client.hold && ud->netconn)
  {
    if (ud->client.hold != 0)
    {
      ud->client.hold = 0;
      netconn_recved(ud->netconn, ud->client.num_held);
      ud->client.num_held = 0;
    }
  }
  return 0;
}

// Lua: client/socket:dns(domain, callback(socket, addr))
static int net_dns( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type == TYPE_TCP_SERVER)
    return luaL_error(L, "invalid user data");
  size_t dl = 0;
  const char *domain = luaL_checklstring(L, 2, &dl);
  if (!domain)
    return luaL_error(L, "no domain specified");
  if (lua_isfunction(L, 3) || lua_islightfunction(L, 3)) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->client.cb_dns_ref);
    lua_pushvalue(L, 3);
    ud->client.cb_dns_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  if (ud->client.cb_dns_ref == LUA_NOREF)
    return luaL_error(L, "no callback specified");

  return lnet_socket_resolve_dns(L, ud, domain, false);
}

// Lua: client:getpeer()
static int net_getpeer( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type != TYPE_TCP_CLIENT)
    return luaL_error(L, "invalid user data");
  if (!ud->netconn) {
    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
  }

  uint16_t port;
  ip_addr_t addr;
  netconn_peer(ud->netconn, &addr, &port);

  if (port == 0) {
    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
  }
  char addr_str[IP_STR_SZ];
  ipstr (addr_str, &addr);
  lua_pushinteger(L, port);
  lua_pushstring(L, addr_str);
  return 2;
}

// Lua: client/server/socket:getaddr()
static int net_getaddr( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud) return luaL_error(L, "invalid user data");
  if (!ud->netconn) {
    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
  }
  uint16_t port = 0;
  ip_addr_t addr;
  switch (ud->type) {
    case TYPE_TCP_CLIENT:
    case TYPE_TCP_SERVER:
    case TYPE_UDP_SOCKET:
      netconn_addr(ud->netconn, &addr, &port);
      break;
    default: break;
  }
  if (port == 0) {
    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
  }
  char addr_str[IP_STR_SZ];
  ipstr (addr_str, &addr);
  lua_pushinteger(L, port);
  lua_pushstring(L, addr_str);
  return 2;
}

// Lua: client/server/socket:close()
static int net_close( lua_State *L ) {
  err_t err = ERR_OK;
  lnet_userdata *ud = net_get_udata(L);
  if (!ud) return luaL_error(L, "invalid user data");
  if (ud->netconn) {
    switch (ud->type) {
      case TYPE_TCP_CLIENT:
      case TYPE_TCP_SERVER:
      case TYPE_UDP_SOCKET:
        ud->closing = true;
        err = netconn_close_and_delete(ud->netconn);
        if (err == ERR_OK)
          ud->netconn = NULL;
        break;
      default: break;
    }
  } else {
    return luaL_error(L, "not connected");
  }
  if (ud->type == TYPE_TCP_SERVER ||
     (ud->netconn == NULL && ud->client.wait_dns == 0)) {
    lua_gc(L, LUA_GCSTOP, 0);
    luaL_unref(L, LUA_REGISTRYINDEX, ud->self_ref);
    ud->self_ref = LUA_NOREF;
    lua_gc(L, LUA_GCRESTART, 0);
  }
  return lwip_lua_checkerr(L, err);
}

static int net_delete( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud) return luaL_error(L, "no user data");
  if (ud->netconn) {
    switch (ud->type) {
      case TYPE_TCP_CLIENT:
      case TYPE_TCP_SERVER:
      case TYPE_UDP_SOCKET:
        ud->closing = true;
        netconn_delete(ud->netconn);
        ud->netconn = NULL;
        break;
      default: break;
    }
  }
  switch (ud->type) {
    case TYPE_TCP_CLIENT:
      luaL_unref(L, LUA_REGISTRYINDEX, ud->client.cb_connect_ref);
      ud->client.cb_connect_ref = LUA_NOREF;
      luaL_unref(L, LUA_REGISTRYINDEX, ud->client.cb_disconnect_ref);
      ud->client.cb_disconnect_ref = LUA_NOREF;
      luaL_unref(L, LUA_REGISTRYINDEX, ud->client.cb_reconnect_ref);
      ud->client.cb_reconnect_ref = LUA_NOREF;
    case TYPE_UDP_SOCKET:
      luaL_unref(L, LUA_REGISTRYINDEX, ud->client.cb_dns_ref);
      ud->client.cb_dns_ref = LUA_NOREF;
      luaL_unref(L, LUA_REGISTRYINDEX, ud->client.cb_receive_ref);
      ud->client.cb_receive_ref = LUA_NOREF;
      luaL_unref(L, LUA_REGISTRYINDEX, ud->client.cb_sent_ref);
      ud->client.cb_sent_ref = LUA_NOREF;
      break;
    case TYPE_TCP_SERVER:
      luaL_unref(L, LUA_REGISTRYINDEX, ud->server.cb_accept_ref);
      ud->server.cb_accept_ref = LUA_NOREF;
      break;
  }
  lua_gc(L, LUA_GCSTOP, 0);
  luaL_unref(L, LUA_REGISTRYINDEX, ud->self_ref);
  ud->self_ref = LUA_NOREF;
  remove_userdata_from_active(ud);
  lua_gc(L, LUA_GCRESTART, 0);
  return 0;
}

// --- Multicast

static int net_multicastJoinLeave( lua_State *L, int join) {
	  size_t il;
	  ip4_addr_t multicast_addr;
	  ip4_addr_t if_addr;
	  const char *multicast_ip;
	  const char *if_ip;

	  NODE_DBG("net_multicastJoin is called.\n");
	  if(! lua_isstring(L,1) ) return luaL_error( L, "wrong arg type" );
	  if_ip = luaL_checklstring( L, 1, &il );
	  if (if_ip != NULL)
		 if ( if_ip[0] == '\0' || strcasecmp(if_ip,"any") == 0)
	     {
			 if_ip = "0.0.0.0";
			 il = 7;
	     }
	  if (if_ip == NULL || il > 15 || il < 7) return luaL_error( L, "invalid if ip" );
    ip4addr_aton (if_ip, &if_addr);

	  if(! lua_isstring(L,2) ) return luaL_error( L, "wrong arg type" );
	  multicast_ip = luaL_checklstring( L, 2, &il );
	  if (multicast_ip == NULL || il > 15 || il < 7) return luaL_error( L, "invalid multicast ip" );
    ip4addr_aton (multicast_ip, &multicast_addr);
	  if (join)
	  {
		  igmp_joingroup(&if_addr, &multicast_addr);
	  }
	  else
	  {
		  igmp_leavegroup(&if_addr, &multicast_addr);
	  }
	  return 0;
}


// Lua: net.multicastJoin(ifip, multicastip)
// if ifip "" or "any" all interfaces are affected
static int net_multicastJoin( lua_State* L ) {
	return net_multicastJoinLeave(L,1);
}


// Lua: net.multicastLeave(ifip, multicastip)
// if ifip "" or "any" all interfaces are affected
static int net_multicastLeave( lua_State* L ) {
	return net_multicastJoinLeave(L,0);
}


// --- DNS ---------------------------------------------------------------


// Lua: net.dns.resolve( domain, function(ip) )
static int net_dns_static( lua_State* L ) {
  size_t dl;
  const char* domain = luaL_checklstring(L, 1, &dl);
  if (!domain && dl > 128) {
    return luaL_error(L, "domain name too long");
  }

  luaL_checkanyfunction(L, 2);
  lua_settop(L, 2);

  dns_event_t *ev = luaM_new(L, dns_event_t);
  if (!ev)
    return luaL_error (L, "out of memory");

  ev->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ev->invoker = DNS_STATIC;

  err_t err = dns_gethostbyname(domain, &ev->resolved_ip, net_dns_cb, ev);
  if (err == ERR_OK)
    net_dns_cb(domain, &ev->resolved_ip, ev);
  else if (err != ERR_INPROGRESS)
  {
    luaM_free(L, ev);
    lwip_lua_checkerr(L, err);
  }
  return 0;
}


// Lua: s = net.dns.setdnsserver(ip_addr, [index])
static int net_setdnsserver( lua_State* L ) {
  size_t l;
  ip_addr_t ipaddr;

  const char *server = luaL_checklstring( L, 1, &l );
  if (l>16 || server == NULL ||
      !ipaddr_aton (server, &ipaddr) || ip_addr_isany (&ipaddr))
    return luaL_error( L, "invalid dns server ip" );

  int numdns = luaL_optint(L, 2, 0);
  if (numdns >= DNS_MAX_SERVERS)
    return luaL_error( L, "server index out of range [0-%d]", DNS_MAX_SERVERS - 1);

  dns_setserver(numdns,&ipaddr);

  return 0;
}


// Lua: s = net.dns.getdnsserver([index])
static int net_getdnsserver( lua_State* L ) {
  int numdns = luaL_optint(L, 1, 0);
  if (numdns >= DNS_MAX_SERVERS)
    return luaL_error( L, "server index out of range [0-%d]", DNS_MAX_SERVERS - 1);

  // ip_addr_t ipaddr;
  // dns_getserver(numdns,&ipaddr);
  // Bug fix by @md5crypt https://github.com/nodemcu/nodemcu-firmware/pull/500
  const ip_addr_t *ipaddr = dns_getserver(numdns);

  if ( ip_addr_isany(ipaddr) ) {
    lua_pushnil( L );
  } else {
    char temp[IP_STR_SZ];
    ipstr (temp, ipaddr);
    lua_pushstring( L, temp );
  }

  return 1;
}


// --- Lua event dispatch -----------------------------------------------

static void ldnsfound_cb (lua_State *L, lnet_userdata *ud, ip_addr_t *addr) {
  if (ud->self_ref != LUA_NOREF && ud->client.cb_dns_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->client.cb_dns_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    if (!ip_addr_isany (addr)) {
      char iptmp[IP_STR_SZ];
      ipstr (iptmp, addr);
      lua_pushstring(L, iptmp);
    } else {
      lua_pushnil(L);
    }
    lua_call(L, 2, 0);
  }
  ud->client.wait_dns --;
  if (ud->netconn && ud->type == TYPE_TCP_CLIENT && !ud->client.connecting) {
    ud->client.connecting = true;
    netconn_connect(ud->netconn, addr, ud->port);
  } else if (!ud->netconn && ud->client.wait_dns == 0) {
    lua_gc(L, LUA_GCSTOP, 0);
    luaL_unref(L, LUA_REGISTRYINDEX, ud->self_ref);
    ud->self_ref = LUA_NOREF;
    lua_gc(L, LUA_GCRESTART, 0);
  }
}


static void ldnsstatic_cb (lua_State *L, int cb_ref, ip_addr_t *addr) {
  if (cb_ref == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, cb_ref);
  luaL_unref (L, LUA_REGISTRYINDEX, cb_ref);

  if (!ip_addr_isany (addr)) {
    char iptmp[IP_STR_SZ];
    ipstr (iptmp, addr);
    lua_pushstring(L, iptmp);
  } else {
    lua_pushnil(L);
  }
  lua_call(L, 1, 0);
}


static void lconnected_cb (lua_State *L, lnet_userdata *ud) {
  if (ud->self_ref != LUA_NOREF && ud->client.cb_connect_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->client.cb_connect_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    lua_call(L, 1, 0);
  }
}


static void lrecv_cb (lua_State *L, lnet_userdata *ud) {
  if (!ud || !ud->netconn) return;

  struct netbuf *p;
  char *payload;
  uint16_t len;

  err_t err = netconn_recv(ud->netconn, &p);
  if (err != ERR_OK || !p) {
    lwip_lua_checkerr(L, err);
    return;
  }

  netbuf_first(p);
  do {
    netbuf_data(p, (void **)&payload, &len);

    if (ud->client.cb_receive_ref != LUA_NOREF){
      lua_rawgeti(L, LUA_REGISTRYINDEX, ud->client.cb_receive_ref);
      int num_args = 2;
      lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
      lua_pushlstring(L, payload, len);
      if (ud->type == TYPE_UDP_SOCKET) {
        num_args += 2;
        char iptmp[IP_STR_SZ];
        ip_addr_t *addr = netbuf_fromaddr(p);
        uint16_t port = netbuf_fromport(p);
        ipstr (iptmp, addr);
        lua_pushinteger(L, port);
        lua_pushstring(L, iptmp);
      }
      lua_call(L, num_args, 0);
    }
  } while (netbuf_next(p) != -1);

  if (p) {
    netbuf_delete(p);

    if (ud->type == TYPE_TCP_CLIENT) {
      if (ud->client.hold) {
        ud->client.num_held += len;
      } else {
        netconn_recved(ud->netconn, len);
      }
    }
  }
}


static void laccept_cb (lua_State *L, lnet_userdata *ud) {
  if (!ud || !ud->netconn) return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, ud->server.cb_accept_ref);

  lnet_userdata *nud = net_create(L, TYPE_TCP_CLIENT);
  lua_pushvalue(L, -1);
  nud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  struct netconn *newconn;
  err_t err = netconn_accept(ud->netconn, &newconn);
  if (err == ERR_OK) {
    nud->netconn = newconn;
    netconn_set_nonblocking(nud->netconn, 1);
    netconn_set_noautorecved(nud->netconn, 1);
    nud->netconn->pcb.tcp->so_options |= SOF_KEEPALIVE;
    nud->netconn->pcb.tcp->keep_idle = ud->server.timeout * 1000;
    nud->netconn->pcb.tcp->keep_cnt = 1;
  } else
    luaL_error(L, "cannot accept new server socket connection");
  lua_call(L, 1, 0);
}


static void lsent_cb (lua_State *L, lnet_userdata *ud) {
  if (ud->client.cb_sent_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->client.cb_sent_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    lua_call(L, 1, 0);
  }
}


static void lerr_cb (lua_State *L, lnet_userdata *ud, err_t err)
{
  if (!ud || !ud->netconn) return;

  int ref;
  if (err != ERR_OK && ud->client.cb_reconnect_ref != LUA_NOREF)
    ref = ud->client.cb_reconnect_ref;
  else ref = ud->client.cb_disconnect_ref;
  if (ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    lua_pushinteger(L, err);
    lua_call(L, 2, 0);
  }
  if (ud->client.wait_dns == 0) {
    lua_gc(L, LUA_GCSTOP, 0);
    luaL_unref(L, LUA_REGISTRYINDEX, ud->self_ref);
    ud->self_ref = LUA_NOREF;
    lua_gc(L, LUA_GCRESTART, 0);
  }
}


// --- Tables

// Module function map
LROT_BEGIN(net_tcpserver)
  LROT_FUNCENTRY( listen,  net_listen )
  LROT_FUNCENTRY( getaddr, net_getaddr )
  LROT_FUNCENTRY( close,   net_close )
  LROT_FUNCENTRY( __gc,    net_delete )
  LROT_TABENTRY ( __index, net_tcpserver )
LROT_END(net_tcpserver, NULL, 0)

LROT_BEGIN(net_tcpsocket)
  LROT_FUNCENTRY( connect, net_connect )
  LROT_FUNCENTRY( close,   net_close )
  LROT_FUNCENTRY( on,      net_on )
  LROT_FUNCENTRY( send,    net_send )
  LROT_FUNCENTRY( hold,    net_hold )
  LROT_FUNCENTRY( unhold,  net_unhold )
  LROT_FUNCENTRY( dns,     net_dns )
  LROT_FUNCENTRY( getpeer, net_getpeer )
  LROT_FUNCENTRY( getaddr, net_getaddr )
  LROT_FUNCENTRY( __gc,    net_delete )
  LROT_TABENTRY ( __index, net_tcpsocket )
LROT_END(net_tcpsocket, NULL, 0)

LROT_BEGIN(net_udpsocket)
  LROT_FUNCENTRY( listen,  net_listen )
  LROT_FUNCENTRY( close,   net_close )
  LROT_FUNCENTRY( on,      net_on )
  LROT_FUNCENTRY( send,    net_send )
  LROT_FUNCENTRY( dns,     net_dns )
  LROT_FUNCENTRY( getaddr, net_getaddr )
  LROT_FUNCENTRY( __gc,    net_delete )
  LROT_TABENTRY ( __index, net_udpsocket )
LROT_END(net_udpsocket, NULL, 0)

LROT_BEGIN(net_dns)
  LROT_FUNCENTRY( setdnsserver, net_setdnsserver )
  LROT_FUNCENTRY( getdnsserver, net_getdnsserver )
  LROT_FUNCENTRY( resolve,      net_dns_static )
LROT_END(net_dns, NULL, 0)

LROT_BEGIN(net)
  LROT_FUNCENTRY( createServer,     net_createServer )
  LROT_FUNCENTRY( createConnection, net_createConnection )
  LROT_FUNCENTRY( createUDPSocket,  net_createUDPSocket )
  LROT_FUNCENTRY( multicastJoin,    net_multicastJoin )
  LROT_FUNCENTRY( multicastLeave,   net_multicastLeave )
  LROT_TABENTRY ( dns,              net_dns )
  LROT_NUMENTRY ( TCP,              TYPE_TCP )
  LROT_NUMENTRY ( UDP,              TYPE_UDP )
  LROT_TABENTRY ( __metatable,      net )
LROT_END(net, NULL, 0)

int luaopen_net( lua_State *L ) {
  igmp_init();

  luaL_rometatable(L, NET_TABLE_TCP_SERVER, (void *)net_tcpserver_map);
  luaL_rometatable(L, NET_TABLE_TCP_CLIENT, (void *)net_tcpsocket_map);
  luaL_rometatable(L, NET_TABLE_UDP_SOCKET, (void *)net_udpsocket_map);

  net_handler = task_get_id(handle_net_event);
  dns_handler = task_get_id(handle_dns_event);

  return 0;
}

NODEMCU_MODULE(NET, "net", net, luaopen_net);
