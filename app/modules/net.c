// Module for network

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "lmem.h"

#include <string.h>
#include <strings.h>
#include <stddef.h>

#include <stdint.h>
#include "mem.h"
#include "osapi.h"
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/dns.h"
#include "lwip/igmp.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/dhcp.h"

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

#pragma mark - LWIP errors

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

#pragma mark - Create

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
      /* FALLTHROUGH */
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

#pragma mark - LWIP callbacks

static void net_err_cb(void *arg, err_t err) {
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud || ud->type != TYPE_TCP_CLIENT || ud->self_ref == LUA_NOREF) return;
  ud->pcb = NULL; // Will be freed at LWIP level
  lua_State *L = lua_getstate();
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
    int selfref = ud->self_ref;
    ud->self_ref = LUA_NOREF;
    luaL_unref(L, LUA_REGISTRYINDEX, selfref);
  }
}

static err_t net_connected_cb(void *arg, struct tcp_pcb *tpcb, err_t err) {
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud || ud->pcb != tpcb) return ERR_ABRT;
  if (err != ERR_OK) {
    net_err_cb(arg, err);
    return ERR_ABRT;
  }
  lua_State *L = lua_getstate();
  if (ud->self_ref != LUA_NOREF && ud->client.cb_connect_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->client.cb_connect_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    lua_call(L, 1, 0);
  }
  return ERR_OK;
}

static void net_dns_cb(const char *name, ip_addr_t *ipaddr, void *arg) {
  ip_addr_t addr;
  if (ipaddr != NULL) addr = *ipaddr;
  else addr.addr = 0xFFFFFFFF;
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud) return;
  lua_State *L = lua_getstate();
  if (ud->self_ref != LUA_NOREF && ud->client.cb_dns_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->client.cb_dns_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    if (addr.addr != 0xFFFFFFFF) {
      char iptmp[16];
      bzero(iptmp, 16);
      ets_sprintf(iptmp, IPSTR, IP2STR(&addr.addr));
      lua_pushstring(L, iptmp);
    } else {
      lua_pushnil(L);
    }
    lua_call(L, 2, 0);
  }
  ud->client.wait_dns --;
  if (ud->pcb && ud->type == TYPE_TCP_CLIENT && ud->tcp_pcb->state == CLOSED) {
    tcp_connect(ud->tcp_pcb, &addr, ud->tcp_pcb->remote_port, net_connected_cb);
  } else if (!ud->pcb && ud->client.wait_dns == 0) {
    int selfref = ud->self_ref;
    ud->self_ref = LUA_NOREF;
    luaL_unref(L, LUA_REGISTRYINDEX, selfref);
  }
}

static void net_recv_cb(lnet_userdata *ud, struct pbuf *p, ip_addr_t *addr, u16_t port) {
  if (ud->client.cb_receive_ref == LUA_NOREF) {
    pbuf_free(p);
    return;
  }

  int num_args = 2;
  char iptmp[16] = { 0, };
  if (ud->type == TYPE_UDP_SOCKET)
  {
    num_args += 2;
    ets_sprintf(iptmp, IPSTR, IP2STR(&addr->addr));
  }

  lua_State *L = lua_getstate();
  struct pbuf *pp = p;
  while (pp)
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->client.cb_receive_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    lua_pushlstring(L, pp->payload, pp->len);
    if (ud->type == TYPE_UDP_SOCKET) {
      lua_pushinteger(L, port);
      lua_pushstring(L, iptmp);
    }
    lua_call(L, num_args, 0);
    pp = pp->next;
  }
  pbuf_free(p);
}

static void net_udp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p, ip_addr_t *addr, u16_t port) {
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud || !ud->pcb || ud->type != TYPE_UDP_SOCKET || ud->self_ref == LUA_NOREF) {
    if (p) pbuf_free(p);
    return;
  }
  net_recv_cb(ud, p, addr, port);
}

static err_t net_tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud || !ud->pcb || ud->type != TYPE_TCP_CLIENT || ud->self_ref == LUA_NOREF)
    return ERR_ABRT;
  if (!p) {
    net_err_cb(arg, err);
    return tcp_close(tpcb);
  }
  net_recv_cb(ud, p, 0, 0);
  tcp_recved(tpcb, ud->client.hold ? 0 : TCP_WND);
  return ERR_OK;
}

static err_t net_sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len) {
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud || !ud->pcb || ud->type != TYPE_TCP_CLIENT || ud->self_ref == LUA_NOREF) return ERR_ABRT;
  if (ud->client.cb_sent_ref == LUA_NOREF) return ERR_OK;
  lua_State *L = lua_getstate();
  lua_rawgeti(L, LUA_REGISTRYINDEX, ud->client.cb_sent_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
  lua_call(L, 1, 0);
  return ERR_OK;
}

static err_t net_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
  lnet_userdata *ud = (lnet_userdata*)arg;
  if (!ud || ud->type != TYPE_TCP_SERVER || !ud->pcb) return ERR_ABRT;
  if (ud->self_ref == LUA_NOREF || ud->server.cb_accept_ref == LUA_NOREF) return ERR_ABRT;

  lua_State *L = lua_getstate();
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

  return net_connected_cb(nud, nud->tcp_pcb, ERR_OK);
}

#pragma mark - Lua API - create

// Lua: net.createUDPSocket()
int net_createUDPSocket( lua_State *L ) {
  net_create(L, TYPE_UDP_SOCKET);
  return 1;
}

// Lua: net.createServer(type, timeout)
int net_createServer( lua_State *L ) {
  int type, timeout;

  timeout = luaL_optlong(L, 1, 30);

  lnet_userdata *u = net_create(L, TYPE_TCP_SERVER);
  u->server.timeout = timeout;
  return 1;
}

// Lua: net.createConnection(type, secure)
int net_createConnection( lua_State *L ) {

  net_create(L, TYPE_TCP_CLIENT);
  return 1;
}

#pragma mark - Get & check userdata

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

#pragma mark - Lua API

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
      ud->tcp_pcb->so_options |= SOF_REUSEADDR;
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
  if (lua_gettop(L) > 3) {
    luaL_argcheck(L, lua_isfunction(L, 4) || lua_islightfunction(L, 4), 4, "not a function");
    lua_pushvalue(L, 4);
    luaL_unref(L, LUA_REGISTRYINDEX, ud->client.cb_connect_ref);
    ud->client.cb_connect_ref = luaL_ref(L, LUA_REGISTRYINDEX);
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
  uint16_t port;
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
    ip_addr_t laddr = {0};
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
  return lwip_lua_checkerr(L, err);
}

// Lua: client:hold()
int net_hold( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type != TYPE_TCP_CLIENT)
    return luaL_error(L, "invalid user data");
  if (!ud->client.hold && ud->tcp_pcb) {
	ud->client.hold = 1;
  }
  return 0;
}

// Lua: client:unhold()
int net_unhold( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type != TYPE_TCP_CLIENT)
    return luaL_error(L, "invalid user data");
  if (ud->client.hold && ud->tcp_pcb) {
	ud->client.hold = 0;
	ud->tcp_pcb->flags |= TF_ACK_NOW;
    tcp_recved(ud->tcp_pcb, TCP_WND);
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

// Lua: client/socket:ttl([ttl])
int net_ttl( lua_State *L ) {
  lnet_userdata *ud = net_get_udata(L);
  if (!ud || ud->type == TYPE_TCP_SERVER)
    return luaL_error(L, "invalid user data");
  if (!ud->pcb)
    return luaL_error(L, "socket is not open/bound yet");
  int ttl = luaL_optinteger(L, 2, -1);
  // Since `ttl` field is part of IP_PCB macro
  // (which are at beginning of both udp_pcb/tcp_pcb)
  // and PCBs declared as `union` there is safe to
  // access ttl field without checking for type.
  if (ttl == -1) {
    ttl = ud->udp_pcb->ttl;
  } else {
    ud->udp_pcb->ttl = ttl;
  }
  lua_pushinteger(L, ttl);
  return 1;
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
  char addr_str[16];
  bzero(addr_str, 16);
  ets_sprintf(addr_str, IPSTR, IP2STR(&addr.addr));
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
  uint16_t port;
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
  char addr_str[16];
  bzero(addr_str, 16);
  ets_sprintf(addr_str, IPSTR, IP2STR(&addr.addr));
  lua_pushinteger(L, port);
  lua_pushstring(L, addr_str);
  return 2;
}
#if 0
static void dbg_print_ud(const char *title, lnet_userdata *ud) {
  int i;
  dbg_printf("%s: Userdata %p:", title, ud);
  for (i=0; i<(sizeof(*ud)/sizeof(uint32_t)); i++) 
    dbg_printf( "  0x%08x", ((uint32_t *)ud)[i]);
  dbg_printf("\n");
#endif

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

    int selfref = ud->self_ref;
    ud->self_ref = LUA_NOREF;
    luaL_unref(L, LUA_REGISTRYINDEX, selfref);
  }
#if 0
  dbg_print_ud("close exit", ud);
#endif
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

  int selfref = ud->self_ref;
  ud->self_ref = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, selfref);
#if 0
  dbg_print_ud("delete end", ud);
#endif
  return 0;
}

#pragma mark - Multicast

static int net_multicastJoinLeave( lua_State *L, int join) {
	  size_t il;
	  ip_addr_t multicast_addr;
	  ip_addr_t if_addr;
	  const char *multicast_ip;
	  const char *if_ip;

	  NODE_DBG("net_multicastJoin is called.\n");
	  if(! lua_isstring(L,1) ) return luaL_error( L, "wrong arg type" );
	  if_ip = luaL_checklstring( L, 1, &il );
	  if (if_ip != NULL)
		 if ( if_ip[0] == '\0' || stricmp(if_ip,"any") == 0)
	     {
			 if_ip = "0.0.0.0";
			 il = 7;
	     }
	  if (if_ip == NULL || il > 15 || il < 7) return luaL_error( L, "invalid if ip" );
	  if_addr.addr = ipaddr_addr(if_ip);

	  if(! lua_isstring(L,2) ) return luaL_error( L, "wrong arg type" );
	  multicast_ip = luaL_checklstring( L, 2, &il );
	  if (multicast_ip == NULL || il > 15 || il < 7) return luaL_error( L, "invalid multicast ip" );
	  multicast_addr.addr = ipaddr_addr(multicast_ip);
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

#pragma mark - DNS

static void net_dns_static_cb(const char *name, ip_addr_t *ipaddr, void *callback_arg) {
  ip_addr_t addr;
  if (ipaddr != NULL)
    addr = *ipaddr;
  else addr.addr = 0xFFFFFFFF;
  int cb_ref = (int)callback_arg;
  lua_State *L = lua_getstate();

  /*
   * Move reference from registry to stack before the call could possibly
   * longjmp us out of here.
   */
  lua_rawgeti(L, LUA_REGISTRYINDEX, cb_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, cb_ref);

  // XXX I have no idea why the API insists on a `nil` here, but it does.
  lua_pushnil(L);

  if (addr.addr != 0xFFFFFFFF) {
    char iptmp[20];
    size_t ipl = sprintf(iptmp, IPSTR, IP2STR(&addr.addr));
    lua_pushlstring(L, iptmp, ipl);
  } else {
    lua_pushnil(L);
  }
  lua_call(L, 2, 0);
}

// Lua: net.dns.resolve( domain, function(sk, ip) )
static int net_dns_static( lua_State* L ) {
  size_t dl;
  const char* domain = luaL_checklstring(L, 1, &dl);
  if (!domain && dl > 128) {
    return luaL_error(L, "wrong domain");
  }

  /* Register callback with registry */
  luaL_checkanyfunction(L, 2);
  lua_pushvalue(L, 2);
  int cbref = luaL_ref(L, LUA_REGISTRYINDEX);
  ip_addr_t addr;

  _Static_assert(sizeof(void *) >= sizeof(typeof(cbref)),
                 "Can't upcast int to ptr");

  err_t err = dns_gethostbyname(domain, &addr, net_dns_static_cb, (void *)cbref);
  if (err == ERR_OK) {
    net_dns_static_cb(domain, &addr, (void *)cbref);
    return 0;
  } else if (err == ERR_INPROGRESS) {
    return 0;
  } else {
    /* Bail out!  Unhook callback from registry, first */
    luaL_unref(L, LUA_REGISTRYINDEX, cbref);
    return lwip_lua_checkerr(L, err);
  }
  return 0;
}

// Lua: s = net.dns.setdnsserver(ip_addr, [index])
static int net_setdnsserver( lua_State* L ) {
  size_t l;
  u32_t ip32;

  const char *server = luaL_checklstring( L, 1, &l );
  if (l>16 || server == NULL || (ip32 = ipaddr_addr(server)) == IPADDR_NONE || ip32 == IPADDR_ANY)
    return luaL_error( L, "invalid dns server ip" );

  int numdns = luaL_optint(L, 2, 0);
  if (numdns >= DNS_MAX_SERVERS)
    return luaL_error( L, "server index out of range [0-%d]", DNS_MAX_SERVERS - 1);

  ip_addr_t ipaddr;
  ip4_addr_set_u32(&ipaddr, ip32);
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
    char temp[20] = {0};
    sprintf(temp, IPSTR, IP2STR( &ipaddr.addr ) );
    lua_pushstring( L, temp );
  }

  return 1;
}

#pragma mark - netif info

/*
 * XXX This is internal to Espressif's SDK, but it's called from several places
 * in the NodeMCU tree.  It would be nicer if there were a LwIP export for this
 * rather than this not-so-secret symbol.
 */
extern struct netif *eagle_lwip_getif(uint8);

static void
push_ipaddr(lua_State *L, ip_addr_t *addr) {
  char temp[20];
  ssize_t ipl = ets_snprintf(temp, sizeof temp, IPSTR, IP2STR(&addr->addr));
  lua_assert (ipl >= 0 && ipl < 20);
  lua_pushlstring( L, temp, ipl );
}

static void
field_from_ipaddr(lua_State *L, const char * field_name, ip_addr_t* addr) {
  if ( ip_addr_isany(addr) ) {
    lua_pushnil(L);
  } else {
    push_ipaddr(L, addr);
  }
  lua_setfield(L, -2, field_name);
}

static int net_ifinfo( lua_State* L ) {
  int ifidx = luaL_optint(L, 1, 0);

  struct netif * nif = eagle_lwip_getif(ifidx);
  if (nif == NULL) {
    return 0;
  }

  lua_createtable(L, 0,
     4 + (nif->dhcp == NULL ? 0 : 1));

  lua_pushlstring(L, nif->hwaddr, nif->hwaddr_len);
  lua_setfield(L, -2, "hwaddr");

  field_from_ipaddr(L, "ip"     , &nif->ip_addr);
  field_from_ipaddr(L, "netmask", &nif->netmask);
  field_from_ipaddr(L, "gateway", &nif->gw);

  if (nif->dhcp != NULL) {
    lua_createtable(L, 0, 3);
    field_from_ipaddr(L, "server_ip" , &nif->dhcp->server_ip_addr  );
    field_from_ipaddr(L, "client_ip" , &nif->dhcp->offered_ip_addr );
    field_from_ipaddr(L, "ntp_server", &nif->dhcp->offered_ntp_addr);
  }
  lua_setfield(L, -2, "dhcp");

  return 1;
}

#pragma mark - Tables

// Module function map
LROT_BEGIN(net_tcpserver)
  LROT_FUNCENTRY( listen, net_listen )
  LROT_FUNCENTRY( getaddr, net_getaddr )
  LROT_FUNCENTRY( close, net_close )
  LROT_FUNCENTRY( __gc, net_delete )
  LROT_TABENTRY( __index, net_tcpserver )
LROT_END( net_tcpserver, net_tcpserver, 0 )


LROT_BEGIN(net_tcpsocket)
  LROT_FUNCENTRY( connect, net_connect )
  LROT_FUNCENTRY( close, net_close )
  LROT_FUNCENTRY( on, net_on )
  LROT_FUNCENTRY( send, net_send )
  LROT_FUNCENTRY( hold, net_hold )
  LROT_FUNCENTRY( unhold, net_unhold )
  LROT_FUNCENTRY( dns, net_dns )
  LROT_FUNCENTRY( ttl, net_ttl )
  LROT_FUNCENTRY( getpeer, net_getpeer )
  LROT_FUNCENTRY( getaddr, net_getaddr )
  LROT_FUNCENTRY( __gc, net_delete )
  LROT_TABENTRY( __index, net_tcpsocket )
LROT_END( net_tcpsocket, net_tcpsocket, 0 )


LROT_BEGIN(net_udpsocket)
  LROT_FUNCENTRY( listen, net_listen )
  LROT_FUNCENTRY( close, net_close )
  LROT_FUNCENTRY( on, net_on )
  LROT_FUNCENTRY( send, net_send )
  LROT_FUNCENTRY( dns, net_dns )
  LROT_FUNCENTRY( ttl, net_ttl )
  LROT_FUNCENTRY( getaddr, net_getaddr )
  LROT_FUNCENTRY( __gc, net_delete )
  LROT_TABENTRY( __index, net_udpsocket )
LROT_END( net_udpsocket, net_udpsocket, 0 )


LROT_BEGIN(net_dns)
  LROT_FUNCENTRY( setdnsserver, net_setdnsserver )
  LROT_FUNCENTRY( getdnsserver, net_getdnsserver )
  LROT_FUNCENTRY( resolve, net_dns_static )
LROT_END( net_dns, net_dns, 0 )

LROT_BEGIN(net)
  LROT_FUNCENTRY( createServer, net_createServer )
  LROT_FUNCENTRY( createConnection, net_createConnection )
  LROT_FUNCENTRY( createUDPSocket, net_createUDPSocket )
  LROT_FUNCENTRY( ifinfo, net_ifinfo )
  LROT_FUNCENTRY( multicastJoin, net_multicastJoin )
  LROT_FUNCENTRY( multicastLeave, net_multicastLeave )
  LROT_TABENTRY( dns, net_dns )
  LROT_TABENTRY( __metatable, net )
LROT_END( net, net, 0 )


int luaopen_net( lua_State *L ) {
  igmp_init();

  luaL_rometatable(L, NET_TABLE_TCP_SERVER, LROT_TABLEREF(net_tcpserver));
  luaL_rometatable(L, NET_TABLE_TCP_CLIENT, LROT_TABLEREF(net_tcpsocket));
  luaL_rometatable(L, NET_TABLE_UDP_SOCKET, LROT_TABLEREF(net_udpsocket));

  return 0;
}

NODEMCU_MODULE(NET, "net", net, luaopen_net);
