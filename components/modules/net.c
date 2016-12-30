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

#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/dns.h" 
#include "lwip/igmp.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"

// Some LWIP macros cause complaints with ptr NULL checks, so shut them off :(
#pragma GCC diagnostic ignored "-Waddress"

typedef enum net_type {
  TYPE_TCP_SERVER = 0,
  TYPE_TCP_CLIENT,
  TYPE_UDP_SOCKET
} net_type;

typedef const char net_table_name[14];

static const net_table_name NET_TABLES[] = {
  "net.tcpserver",
  "net.tcpsocket",
  "net.udpsocket"
};
#define NET_TABLE_TCP_SERVER NET_TABLES[0]
#define NET_TABLE_TCP_CLIENT NET_TABLES[1]
#define NET_TABLE_UDP_SOCKET NET_TABLES[2]

#define TYPE_TCP TYPE_TCP_CLIENT
#define TYPE_UDP TYPE_UDP_SOCKET

typedef struct lnet_userdata {
  enum net_type type;
  int self_ref;
  union {
    struct tcp_pcb *tcp_pcb;
    struct udp_pcb *udp_pcb;
    void *pcb;
  };
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
      int hold;
      int cb_connect_ref;
      int cb_disconnect_ref;
      int cb_reconnect_ref;
    } client;
  };
} lnet_userdata;



// --- Event handling

typedef struct {
  ip_addr_t src_ip;
  uint16_t src_port;
  uint16_t payload_len;
  char payload[0];
} lnet_recvdata;


typedef struct {
  enum {
    DNSFOUND,
    DNSSTATIC,
    CONNECTED,
    ACCEPT,
    RECVDATA,
    SENTDATA,
    ERR
  } event;
  union {
    lnet_userdata *ud;
    int cb_ref;
  };
  union {
    struct tcp_pcb *accept_newpcb;
    lnet_recvdata   recvdata;
    ip_addr_t       resolved_ip;
    int             err;
  };
} lnet_event;

static task_handle_t net_event;


// --- LWIP errors

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

// --- Create

lnet_userdata *net_create( lua_State *L, enum net_type type ) {
  const char *mt = NET_TABLES[type];
  lnet_userdata *ud = (lnet_userdata *)lua_newuserdata(L, sizeof(lnet_userdata));
  if (!ud) return NULL;
  luaL_getmetatable(L, mt);
  lua_setmetatable(L, -2);

  ud->type = type;
  ud->self_ref = LUA_NOREF;
  ud->pcb = NULL;

  switch (type) {
    case TYPE_TCP_CLIENT:
      ud->client.cb_connect_ref = LUA_NOREF;
      ud->client.cb_reconnect_ref = LUA_NOREF;
      ud->client.cb_disconnect_ref = LUA_NOREF;
      ud->client.hold = 0;
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
  return ud;
}

// --- LWIP callbacks and task_post helpers

static bool post_net_err (lnet_userdata *ud, err_t err) {
  lnet_event *ev = (lnet_event *)malloc (sizeof (lnet_event));
  if (!ev)
    return false;
  ev->event = ERR;
  ev->ud = ud;
  ev->err = err;
  if (!task_post_medium (net_event, (task_param_t)ev)) {
    free (ev);
    return false;
  }
  return true;
}

static void net_err_cb(void *arg, err_t err) {
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud || ud->type != TYPE_TCP_CLIENT || ud->self_ref == LUA_NOREF) return;
  ud->pcb = NULL; // Will be freed at LWIP level

  post_net_err (ud, err);
}


static bool post_net_connected (lnet_userdata *ud) {
  lnet_event *ev = (lnet_event *)malloc (sizeof (lnet_event));
  if (!ev)
    return false;
  ev->event = CONNECTED;
  ev->ud = ud;
  if (!task_post_medium (net_event, (task_param_t)ev)) {
    free (ev);
    return false;
  }
  return true;
}

static err_t net_connected_cb(void *arg, struct tcp_pcb *tpcb, err_t err) {
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud || ud->pcb != tpcb) return ERR_ABRT;
  if (err != ERR_OK) {
    net_err_cb(arg, err);
    return ERR_ABRT;
  }
  post_net_connected (ud);
  return ERR_OK;
}


static bool post_net_dns (lnet_userdata *ud, const char *name, const ip_addr_t *ipaddr)
{
  lnet_event *ev = (lnet_event *)malloc (sizeof (lnet_event));
  if (!ev || !ipaddr)
    return false;
  ev->event = DNSFOUND;
  ev->ud = ud;
  ev->resolved_ip = *ipaddr;
  if (!task_post_medium (net_event, (task_param_t)ev)) {
    free (ev);
    return false;
  }
  return true;
}

static void net_dns_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
  ip_addr_t addr;
  if (ipaddr != NULL) addr = *ipaddr;
  else addr = ip_addr_any;
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud) return;

  post_net_dns (ud, name, &addr);
}


static bool post_net_recv (lnet_userdata *ud, struct pbuf *p, const ip_addr_t *ip, u16_t port)
{
  lnet_event *ev = (lnet_event *)malloc (sizeof (lnet_event) + p->len);
  if (!ev)
    return false;

  ev->event = RECVDATA;
  ev->ud = ud;

  if (ip)
    ev->recvdata.src_ip = *ip;
  ev->recvdata.src_port = port;
  ev->recvdata.payload_len = p->len;
  pbuf_copy_partial (p, &ev->recvdata.payload, p->len, 0);

  if (!task_post_high (net_event, (task_param_t)ev))
  {
    free (ev);
    return false;
  }
  return true;
}

static void net_udp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud || !ud->pcb || ud->type != TYPE_UDP_SOCKET || ud->self_ref == LUA_NOREF) {
    if (p) pbuf_free(p);
    return;
  }
  post_net_recv (ud, p, addr, port);
}

static err_t net_tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud || !ud->pcb || ud->type != TYPE_TCP_CLIENT || ud->self_ref == LUA_NOREF)
    return ERR_ABRT;
  if (!p) {
    net_err_cb(arg, err);
    return tcp_close(tpcb);
  }

  if (post_net_recv (ud, p, 0, 0))
    tcp_recved(tpcb, ud->client.hold ? 0 : TCP_WND(tpcb));

  return ERR_OK;
}


static bool post_net_sent (lnet_userdata *ud) {
  lnet_event *ev = (lnet_event *)malloc (sizeof (lnet_event));
  if (!ev)
    return false;
  ev->event = SENTDATA;
  ev->ud = ud;
  if (!task_post_medium (net_event, (task_param_t)ev)) {
    free (ev);
    return false;
  }
  return true;
}

static err_t net_sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len) {
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud || !ud->pcb || ud->type != TYPE_TCP_CLIENT || ud->self_ref == LUA_NOREF) return ERR_ABRT;
  if (ud->client.cb_sent_ref == LUA_NOREF) return ERR_OK;

  post_net_sent (ud);
  // TODO: if we can't post this, we effectively stall this socket, how to fix?
  return ERR_OK;
}


static bool post_net_accept (lnet_userdata *ud, struct tcp_pcb *newpcb) {
  lnet_event *ev = (lnet_event *)malloc (sizeof (lnet_event));
  if (!ev)
    return false;
  ev->event = SENTDATA;
  ev->ud = ud;
  ev->accept_newpcb = newpcb;
  if (!task_post_medium (net_event, (task_param_t)ev)) {
    free (ev);
    return false;
  }
  return true;
}


static err_t net_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud || ud->type != TYPE_TCP_SERVER || !ud->pcb) return ERR_ABRT;
  if (ud->self_ref == LUA_NOREF || ud->server.cb_accept_ref == LUA_NOREF) return ERR_ABRT;

  return post_net_accept (ud, newpcb) ? ERR_OK : ERR_ABRT;
}

// --- Lua API - create

extern int tls_socket_create( lua_State *L );

// Lua: net.createUDPSocket()
int net_createUDPSocket( lua_State *L ) {
  net_create(L, TYPE_UDP_SOCKET);
  return 1;
}

// Lua: net.createServer(type, timeout)
int net_createServer( lua_State *L ) {
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
int net_createConnection( lua_State *L ) {
  int type, secure;

  type = luaL_optlong(L, 1, TYPE_TCP);
  secure = luaL_optlong(L, 2, 0);

  if (type == TYPE_UDP) return net_createUDPSocket( L );
  if (type != TYPE_TCP) return luaL_error(L, "invalid type");
  if (secure) {
#ifdef CLIENT_SSL_ENABLE
    return tls_socket_create( L );
#else
    return luaL_error(L, "secure connections not enabled");
#endif
  }
  net_create(L, TYPE_TCP_CLIENT);
  return 1;
}

// --- Get & check userdata

lnet_userdata *net_get_udata_s( lua_State *L, int stack ) {
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

// --- Lua API

// Lua: server:listen(port, addr, function(c)), socket:listen(port, addr)
int net_listen( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type == TYPE_TCP_CLIENT)
    return luaL_error(L, "invalid user data");
  if (ud->pcb)
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
      ud->tcp_pcb = tcp_new();
      if (!ud->tcp_pcb)
        return luaL_error(L, "cannot allocate PCB");
      err = tcp_bind(ud->tcp_pcb, &addr, port);
      if (err == ERR_OK) {
        tcp_arg(ud->tcp_pcb, ud);
        struct tcp_pcb *pcb = tcp_listen(ud->tcp_pcb);
        if (!pcb) {
          err = ERR_MEM;
        } else {
          ud->tcp_pcb = pcb;
          tcp_accept(ud->tcp_pcb, net_accept_cb);
        }
      }
      break;
    case TYPE_UDP_SOCKET:
      ud->udp_pcb = udp_new();
      if (!ud->udp_pcb)
        return luaL_error(L, "cannot allocate PCB");
      udp_recv(ud->udp_pcb, net_udp_recv_cb, ud);
      err = udp_bind(ud->udp_pcb, &addr, port);
      break;
    default: break;
  }
  if (err != ERR_OK) {
    switch (ud->type) {
      case TYPE_TCP_SERVER:
        tcp_close(ud->tcp_pcb);
        ud->tcp_pcb = NULL;
        break;
      case TYPE_UDP_SOCKET:
        udp_remove(ud->udp_pcb);
        ud->udp_pcb = NULL;
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
int net_connect( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type != TYPE_TCP_CLIENT)
    return luaL_error(L, "invalid user data");
  if (ud->pcb)
    return luaL_error(L, "already connected");
  uint16_t port = luaL_checkinteger(L, 2);
  if (port == 0)
    return luaL_error(L, "specify port");
  const char *domain = "127.0.0.1";
  if (lua_isstring(L, 3)) {
    size_t dl = 0;
    domain = luaL_checklstring(L, 3, &dl);
  }
  ud->tcp_pcb = tcp_new();
  if (!ud->tcp_pcb)
    return luaL_error(L, "cannot allocate PCB");
  tcp_arg(ud->tcp_pcb, ud);
  tcp_err(ud->tcp_pcb, net_err_cb);
  tcp_recv(ud->tcp_pcb, net_tcp_recv_cb);
  tcp_sent(ud->tcp_pcb, net_sent_cb);
  ud->tcp_pcb->remote_port = port;
  ip_addr_t addr;
  ud->client.wait_dns ++;
  int unref = 0;
  if (ud->self_ref == LUA_NOREF) {
    unref = 1;
    lua_pushvalue(L, 1);
    ud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  err_t err = dns_gethostbyname(domain, &addr, net_dns_cb, ud);
  if (err == ERR_OK) {
    net_dns_cb(domain, &addr, ud);
  } else if (err != ERR_INPROGRESS) {
    ud->client.wait_dns --;
    if (unref) {
      luaL_unref(L, LUA_REGISTRYINDEX, ud->self_ref);
      ud->self_ref = LUA_NOREF;
    }
    tcp_abort(ud->tcp_pcb);
    ud->tcp_pcb = NULL;
    return lwip_lua_checkerr(L, err);
  }
  return 0;
}

// Lua: client/socket:on(name, callback)
int net_on( lua_State *L ) {
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
int net_send( lua_State *L ) {
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
  if (lua_isfunction(L, stack) || lua_islightfunction(L, stack)) {
    lua_pushvalue(L, stack++);
    luaL_unref(L, LUA_REGISTRYINDEX, ud->client.cb_sent_ref);
    ud->client.cb_sent_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  if (ud->type == TYPE_UDP_SOCKET && !ud->pcb) {
    ud->udp_pcb = udp_new();
    if (!ud->udp_pcb)
      return luaL_error(L, "cannot allocate PCB");
    udp_recv(ud->udp_pcb, net_udp_recv_cb, ud);
    ip_addr_t laddr = IPADDR_ANY_TYPE_INIT;
    err_t err = udp_bind(ud->udp_pcb, &laddr, 0);
    if (err != ERR_OK) {
      udp_remove(ud->udp_pcb);
      ud->udp_pcb = NULL;
      return lwip_lua_checkerr(L, err);
    }
    if (ud->self_ref == LUA_NOREF) {
      lua_pushvalue(L, 1);
      ud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
  }
  if (!ud->pcb || ud->self_ref == LUA_NOREF)
    return luaL_error(L, "not connected");
  err_t err;
  if (ud->type == TYPE_UDP_SOCKET) {
    struct pbuf *pb = pbuf_alloc(PBUF_TRANSPORT, datalen, PBUF_RAM);
    if (!pb)
      return luaL_error(L, "cannot allocate message buffer");
    pbuf_take(pb, data, datalen);
    err = udp_sendto(ud->udp_pcb, pb, &addr, port);
    pbuf_free(pb);
    if (ud->client.cb_sent_ref != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, ud->client.cb_sent_ref);
      lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
      lua_call(L, 1, 0);
    }
  } else if (ud->type == TYPE_TCP_CLIENT) {
    err = tcp_write(ud->tcp_pcb, data, datalen, TCP_WRITE_FLAG_COPY);
  }
  else {
    err = ERR_VAL;
  }
  return lwip_lua_checkerr(L, err);
}

// Lua: client:hold()
int net_hold( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type != TYPE_TCP_CLIENT)
    return luaL_error(L, "invalid user data");
  if (!ud->client.hold && ud->tcp_pcb)
  {
    ud->client.hold = 1;
  }
  return 0;
}

// Lua: client:unhold()
int net_unhold( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type != TYPE_TCP_CLIENT)
    return luaL_error(L, "invalid user data");
  if (ud->client.hold && ud->tcp_pcb)
  {
    ud->client.hold = 0;
    ud->tcp_pcb->flags |= TF_ACK_NOW;
    tcp_recved(ud->tcp_pcb, TCP_WND(ud->tcp_pcb));
  }
  return 0;
}

// Lua: client/socket:dns(domain, callback(socket, addr))
int net_dns( lua_State *L ) {
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
  ud->client.wait_dns ++;
  int unref = 0;
  if (ud->self_ref == LUA_NOREF) {
    unref = 1;
    lua_pushvalue(L, 1);
    ud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  ip_addr_t addr;
  err_t err = dns_gethostbyname(domain, &addr, net_dns_cb, ud);
  if (err == ERR_OK) {
    net_dns_cb(domain, &addr, ud);
  } else if (err != ERR_INPROGRESS) {
    ud->client.wait_dns --;
    if (unref) {
      luaL_unref(L, LUA_REGISTRYINDEX, ud->self_ref);
      ud->self_ref = LUA_NOREF;
    }
    return lwip_lua_checkerr(L, err);
  }
  return 0;
}

// Lua: client:getpeer()
int net_getpeer( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type != TYPE_TCP_CLIENT)
    return luaL_error(L, "invalid user data");
  if (!ud->pcb) {
    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
  }
  uint16_t port = ud->tcp_pcb->remote_port;
  ip_addr_t addr = ud->tcp_pcb->remote_ip;
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
int net_getaddr( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud) return luaL_error(L, "invalid user data");
  if (!ud->pcb) {
    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
  }
  uint16_t port = 0;
  ip_addr_t addr;
  switch (ud->type) {
    case TYPE_TCP_CLIENT:
    case TYPE_TCP_SERVER:
      addr = ud->tcp_pcb->local_ip;
      port = ud->tcp_pcb->local_port;
      break;
    case TYPE_UDP_SOCKET:
      addr = ud->udp_pcb->local_ip;
      port = ud->udp_pcb->local_port;
      break;
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
int net_close( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud) return luaL_error(L, "invalid user data");
  if (ud->pcb) {
    switch (ud->type) {
      case TYPE_TCP_CLIENT:
        if (ERR_OK != tcp_close(ud->tcp_pcb)) {
          tcp_arg(ud->tcp_pcb, NULL);
          tcp_abort(ud->tcp_pcb);
        }
        ud->tcp_pcb = NULL;
        break;
      case TYPE_TCP_SERVER:
        tcp_close(ud->tcp_pcb);
        ud->tcp_pcb = NULL;
        break;
      case TYPE_UDP_SOCKET:
        udp_remove(ud->udp_pcb);
        ud->udp_pcb = NULL;
        break;
    }
  } else {
    return luaL_error(L, "not connected");
  }
  if (ud->type == TYPE_TCP_SERVER ||
     (ud->pcb == NULL && ud->client.wait_dns == 0)) {
    lua_gc(L, LUA_GCSTOP, 0);
    luaL_unref(L, LUA_REGISTRYINDEX, ud->self_ref);
    ud->self_ref = LUA_NOREF;
    lua_gc(L, LUA_GCRESTART, 0);
  }
  return 0;
}

int net_delete( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud) return luaL_error(L, "no user data");
  if (ud->pcb) {
    switch (ud->type) {
      case TYPE_TCP_CLIENT:
        tcp_arg(ud->tcp_pcb, NULL);
        tcp_abort(ud->tcp_pcb);
        ud->tcp_pcb = NULL;
        break;
      case TYPE_TCP_SERVER:
        tcp_close(ud->tcp_pcb);
        ud->tcp_pcb = NULL;
        break;
      case TYPE_UDP_SOCKET:
        udp_remove(ud->udp_pcb);
        ud->udp_pcb = NULL;
        break;
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

// --- DNS

static void net_dns_static_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
  lnet_event *ev = (lnet_event *)callback_arg;

  ev->resolved_ip = ipaddr ? *ipaddr : ip_addr_any;

  if (!task_post_medium (net_event, (task_param_t)ev))
    free (ev);
}

// Lua: net.dns.resolve( domain, function(ip) )
static int net_dns_static( lua_State* L ) {
  size_t dl;
  const char* domain = luaL_checklstring(L, 1, &dl);
  if (!domain && dl > 128) {
    return luaL_error(L, "wrong domain");
  }

  // Note: this will be free'd using regular free(), so can't luaM_malloc()
  lnet_event *ev = (lnet_event *)malloc (sizeof (lnet_event));
  if (!ev)
    return luaL_error (L, "out of memory");

  ev->event = DNSSTATIC;

  luaL_checkanyfunction(L, 2);
  lua_pushvalue(L, 2);  // copy argument (func) to the top of stack
  ev->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  err_t err = dns_gethostbyname(
    domain, &ev->resolved_ip, net_dns_static_cb, ev);
  if (err == ERR_OK) {
    net_dns_static_cb(domain, &ev->resolved_ip, ev);
    return 0;
  } else if (err == ERR_INPROGRESS) {
    return 0;
  } else {
    int e = lwip_lua_checkerr(L, err);
    free(ev);
    return e;
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
  ip_addr_t ipaddr = dns_getserver(numdns);

  if ( ip_addr_isany(&ipaddr) ) {
    lua_pushnil( L );
  } else {
    char temp[IP_STR_SZ];
    ipstr (temp, &ipaddr);
    lua_pushstring( L, temp );
  }

  return 1;
}

// --- Lua event dispatch

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
  if (ud->pcb && ud->type == TYPE_TCP_CLIENT && ud->tcp_pcb->state == CLOSED) {
    tcp_connect(ud->tcp_pcb, addr, ud->tcp_pcb->remote_port, net_connected_cb);
  } else if (!ud->pcb && ud->client.wait_dns == 0) {
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

static void laccept_cb (lua_State *L, lnet_userdata *ud, struct tcp_pcb *newpcb) {
  lua_rawgeti(L, LUA_REGISTRYINDEX, ud->server.cb_accept_ref);

  lnet_userdata *nud = net_create(L, TYPE_TCP_CLIENT);
  lua_pushvalue(L, 2);
  nud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  nud->tcp_pcb = newpcb;
  tcp_arg(nud->tcp_pcb, nud);
  tcp_err(nud->tcp_pcb, net_err_cb);
  tcp_recv(nud->tcp_pcb, net_tcp_recv_cb);
  tcp_sent(nud->tcp_pcb, net_sent_cb);
  nud->tcp_pcb->so_options |= SOF_KEEPALIVE;
  nud->tcp_pcb->keep_idle = ud->server.timeout * 1000;
  nud->tcp_pcb->keep_cnt = 1;
  tcp_accepted(ud->tcp_pcb);

  lua_call(L, 1, 0);
}

static void lrecv_cb (lua_State *L, lnet_userdata *ud, const lnet_recvdata *rd) {
  if (ud->client.cb_receive_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->client.cb_receive_ref);
    int num_args = 2;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    lua_pushlstring(L, rd->payload, rd->payload_len);
    if (ud->type == TYPE_UDP_SOCKET) {
      num_args += 2;
      char iptmp[IP_STR_SZ];
      ipstr (iptmp, &rd->src_ip);
      lua_pushinteger(L, rd->src_port);
      lua_pushstring(L, iptmp);
    }
    lua_call(L, num_args, 0);
  }
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

static void handle_net_event (task_param_t param, task_prio_t prio)
{
  lnet_event *ev = (lnet_event *)param;
  (void)prio;

  lua_State *L = lua_getstate();
  switch (ev->event)
  {
    case DNSFOUND:  ldnsfound_cb (L, ev->ud, &ev->resolved_ip);      break;
    case DNSSTATIC: ldnsstatic_cb (L, ev->cb_ref, &ev->resolved_ip); break;
    case CONNECTED: lconnected_cb (L, ev->ud);                       break;
    case ACCEPT:    laccept_cb (L, ev->ud, ev->accept_newpcb);       break;
    case RECVDATA:  lrecv_cb (L, ev->ud, &ev->recvdata);             break;
    case SENTDATA:  lsent_cb (L, ev->ud);                            break;
    case ERR:       lerr_cb (L, ev->ud, ev->err);                    break;
  }

  free (ev);
}

// --- Tables

extern const LUA_REG_TYPE tls_cert_map[];

// Module function map
static const LUA_REG_TYPE net_tcpserver_map[] = {
  { LSTRKEY( "listen" ),  LFUNCVAL( net_listen ) },
  { LSTRKEY( "getaddr" ), LFUNCVAL( net_getaddr ) },
  { LSTRKEY( "close" ),   LFUNCVAL( net_close ) },
  { LSTRKEY( "__gc" ),    LFUNCVAL( net_delete ) },
  { LSTRKEY( "__index" ), LROVAL( net_tcpserver_map ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE net_tcpsocket_map[] = {
  { LSTRKEY( "connect" ), LFUNCVAL( net_connect ) },
  { LSTRKEY( "close" ),   LFUNCVAL( net_close ) },
  { LSTRKEY( "on" ),      LFUNCVAL( net_on ) },
  { LSTRKEY( "send" ),    LFUNCVAL( net_send ) },
  { LSTRKEY( "hold" ),    LFUNCVAL( net_hold ) },
  { LSTRKEY( "unhold" ),  LFUNCVAL( net_unhold ) },
  { LSTRKEY( "dns" ),     LFUNCVAL( net_dns ) },
  { LSTRKEY( "getpeer" ), LFUNCVAL( net_getpeer ) },
  { LSTRKEY( "getaddr" ), LFUNCVAL( net_getaddr ) },
  { LSTRKEY( "__gc" ),    LFUNCVAL( net_delete ) },
  { LSTRKEY( "__index" ), LROVAL( net_tcpsocket_map ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE net_udpsocket_map[] = {
  { LSTRKEY( "listen" ),  LFUNCVAL( net_listen ) },
  { LSTRKEY( "close" ),   LFUNCVAL( net_close ) },
  { LSTRKEY( "on" ),      LFUNCVAL( net_on ) },
  { LSTRKEY( "send" ),    LFUNCVAL( net_send ) },
  { LSTRKEY( "dns" ),     LFUNCVAL( net_dns ) },
  { LSTRKEY( "getaddr" ), LFUNCVAL( net_getaddr ) },
  { LSTRKEY( "__gc" ),    LFUNCVAL( net_delete ) },
  { LSTRKEY( "__index" ), LROVAL( net_udpsocket_map ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE net_dns_map[] = {
  { LSTRKEY( "setdnsserver" ), LFUNCVAL( net_setdnsserver ) },
  { LSTRKEY( "getdnsserver" ), LFUNCVAL( net_getdnsserver ) },
  { LSTRKEY( "resolve" ),      LFUNCVAL( net_dns_static ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE net_map[] = {
  { LSTRKEY( "createServer" ),     LFUNCVAL( net_createServer ) },
  { LSTRKEY( "createConnection" ), LFUNCVAL( net_createConnection ) },
  { LSTRKEY( "createUDPSocket" ),  LFUNCVAL( net_createUDPSocket ) },
  { LSTRKEY( "multicastJoin"),     LFUNCVAL( net_multicastJoin ) },
  { LSTRKEY( "multicastLeave"),    LFUNCVAL( net_multicastLeave ) },
  { LSTRKEY( "dns" ),              LROVAL( net_dns_map ) },
#ifdef CLIENT_SSL_ENABLE
  { LSTRKEY( "cert" ),             LROVAL( tls_cert_map ) },
#endif
  { LSTRKEY( "TCP" ),              LNUMVAL( TYPE_TCP ) },
  { LSTRKEY( "UDP" ),              LNUMVAL( TYPE_UDP ) },
  { LSTRKEY( "__metatable" ),      LROVAL( net_map ) },
  { LNILKEY, LNILVAL }
};

int luaopen_net( lua_State *L ) {
  igmp_init();

  luaL_rometatable(L, NET_TABLE_TCP_SERVER, (void *)net_tcpserver_map);
  luaL_rometatable(L, NET_TABLE_TCP_CLIENT, (void *)net_tcpsocket_map);
  luaL_rometatable(L, NET_TABLE_UDP_SOCKET, (void *)net_udpsocket_map);

  net_event = task_get_id (handle_net_event);

  return 0;
}

NODEMCU_MODULE(NET, "net", net_map, luaopen_net);
