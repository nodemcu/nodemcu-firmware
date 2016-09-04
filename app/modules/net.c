// Module for network

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "lmem.h"

#include "c_string.h"
#include "c_stdlib.h"

#include "c_types.h"
#include "mem.h"
#include "lwip/ip_addr.h"
#include "espconn.h"
#include "lwip/dns.h" 

#define TCP ESPCONN_TCP
#define UDP ESPCONN_UDP

static ip_addr_t host_ip; // for dns

#ifdef HAVE_SSL_SERVER_CRT
#include HAVE_SSL_SERVER_CRT
#else
__attribute__((section(".servercert.flash"))) unsigned char net_server_cert_area[INTERNAL_FLASH_SECTOR_SIZE];
#endif

__attribute__((section(".clientcert.flash"))) unsigned char net_client_cert_area[INTERNAL_FLASH_SECTOR_SIZE];

#if 0
static int expose_array(lua_State* L, char *array, unsigned short len);
#endif

#define MAX_SOCKET 5
static int socket_num = 0;
static int socket[MAX_SOCKET];
static int tcpserver_cb_connect_ref = LUA_NOREF;  // for tcp server connected callback
static uint16_t tcp_server_timeover = 30;

static struct espconn *pTcpServer = NULL;
static struct espconn *pUdpServer = NULL;

typedef struct lnet_userdata
{
  struct espconn *pesp_conn;
  int self_ref;
  int cb_connect_ref;
  int cb_reconnect_ref;
  int cb_disconnect_ref;
  int cb_receive_ref;
  int cb_send_ref;
  int cb_dns_found_ref;
#ifdef CLIENT_SSL_ENABLE
  uint8_t secure;
#endif
}lnet_userdata;

static void net_server_disconnected(void *arg)    // for tcp server only
{
  NODE_DBG("net_server_disconnected is called.\n");
  struct espconn *pesp_conn = arg;
  if(pesp_conn == NULL)
    return;
  lnet_userdata *nud = (lnet_userdata *)pesp_conn->reverse;
  if(nud == NULL)
    return;
  lua_State *L = lua_getstate();
#if 0
  char temp[20] = {0};
  c_sprintf(temp, IPSTR, IP2STR( &(pesp_conn->proto.tcp->remote_ip) ) );
  NODE_DBG("remote ");
  NODE_DBG(temp);
  NODE_DBG(":");
  NODE_DBG("%d",pesp_conn->proto.tcp->remote_port);
  NODE_DBG(" disconnected.\n");
#endif
  if(nud->cb_disconnect_ref != LUA_NOREF && nud->self_ref != LUA_NOREF)
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, nud->cb_disconnect_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, nud->self_ref);  // pass the userdata(client) to callback func in lua
    lua_call(L, 1, 0);
  }
  int i;
  lua_gc(L, LUA_GCSTOP, 0);
  for(i=0;i<MAX_SOCKET;i++){
    if( (LUA_NOREF!=socket[i]) && (socket[i] == nud->self_ref) ){
      // found the saved client
      nud->pesp_conn->reverse = NULL;
      nud->pesp_conn = NULL;    // the espconn is made by low level sdk, do not need to free, delete() will not free it.
      nud->self_ref = LUA_NOREF;   // unref this, and the net.socket userdata will delete it self
      luaL_unref(L, LUA_REGISTRYINDEX, socket[i]);
      socket[i] = LUA_NOREF;
      socket_num--;
      break;
    }
  }
  lua_gc(L, LUA_GCRESTART, 0);
}

static void net_socket_disconnected(void *arg)    // tcp only
{
  NODE_DBG("net_socket_disconnected is called.\n");
  struct espconn *pesp_conn = arg;
  if(pesp_conn == NULL)
    return;
  lnet_userdata *nud = (lnet_userdata *)pesp_conn->reverse;
  if(nud == NULL)
    return;
  lua_State *L = lua_getstate();
  if(nud->cb_disconnect_ref != LUA_NOREF && nud->self_ref != LUA_NOREF)
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, nud->cb_disconnect_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, nud->self_ref);  // pass the userdata(client) to callback func in lua
    lua_call(L, 1, 0);
  }

  if(pesp_conn->proto.tcp)
    c_free(pesp_conn->proto.tcp);
  pesp_conn->proto.tcp = NULL;
  if(nud->pesp_conn)
    c_free(nud->pesp_conn);
  nud->pesp_conn = NULL;  // espconn is already disconnected
  lua_gc(L, LUA_GCSTOP, 0);
  if(nud->self_ref != LUA_NOREF){
    luaL_unref(L, LUA_REGISTRYINDEX, nud->self_ref);
    nud->self_ref = LUA_NOREF; // unref this, and the net.socket userdata will delete it self
  }
  lua_gc(L, LUA_GCRESTART, 0);
}

static void net_server_reconnected(void *arg, sint8_t err)
{
  NODE_DBG("net_server_reconnected is called.\n");
  net_server_disconnected(arg);
}

static void net_socket_reconnected(void *arg, sint8_t err)
{
  NODE_DBG("net_socket_reconnected is called.\n");
  net_socket_disconnected(arg);
}

static void net_socket_received(void *arg, char *pdata, unsigned short len)
{
  NODE_DBG("net_socket_received is called.\n");
  struct espconn *pesp_conn = arg;
  if(pesp_conn == NULL)
    return;
  lnet_userdata *nud = (lnet_userdata *)pesp_conn->reverse;
  if(nud == NULL)
    return;
  if(nud->cb_receive_ref == LUA_NOREF)
    return;
  if(nud->self_ref == LUA_NOREF)
    return;
  lua_State *L = lua_getstate();
  lua_rawgeti(L, LUA_REGISTRYINDEX, nud->cb_receive_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, nud->self_ref);  // pass the userdata(server) to callback func in lua
  // expose_array(L, pdata, len);
  // *(pdata+len) = 0;
  // NODE_DBG(pdata);
  // NODE_DBG("\n");
  lua_pushlstring(L, pdata, len);
  // lua_pushinteger(L, len);
  lua_call(L, 2, 0);
}

static void net_socket_sent(void *arg)
{
  // NODE_DBG("net_socket_sent is called.\n");
  struct espconn *pesp_conn = arg;
  if(pesp_conn == NULL)
    return;
  lnet_userdata *nud = (lnet_userdata *)pesp_conn->reverse;
  if(nud == NULL)
    return;
  if(nud->cb_send_ref == LUA_NOREF)
    return;
  if(nud->self_ref == LUA_NOREF)
    return;
  lua_State *L = lua_getstate();
  lua_rawgeti(L, LUA_REGISTRYINDEX, nud->cb_send_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, nud->self_ref);  // pass the userdata(server) to callback func in lua
  lua_call(L, 1, 0);
}

static void net_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
  NODE_DBG("net_dns_found is called.\n");
  struct espconn *pesp_conn = arg;
  if(pesp_conn == NULL){
    NODE_DBG("pesp_conn null.\n");
    return;
  }
  lnet_userdata *nud = (lnet_userdata *)pesp_conn->reverse;
  if(nud == NULL){
    NODE_DBG("nud null.\n");
    return;
  }
  if(nud->cb_dns_found_ref == LUA_NOREF){
    NODE_DBG("cb_dns_found_ref null.\n");
    return;
  }

  if(nud->self_ref == LUA_NOREF){
    NODE_DBG("self_ref null.\n");
    return;
  }

  lua_State *L = lua_getstate();

  lua_rawgeti(L, LUA_REGISTRYINDEX, nud->cb_dns_found_ref);    // the callback function
  lua_rawgeti(L, LUA_REGISTRYINDEX, nud->self_ref);  // pass the userdata(conn) to callback func in lua

  if(ipaddr == NULL)
  {
    NODE_DBG( "DNS Fail!\n" );
    lua_pushnil(L);
  }else{
    // ipaddr->addr is a uint32_t ip
    char ip_str[20];
    c_memset(ip_str, 0, sizeof(ip_str));
    if(ipaddr->addr != 0)
    {
      c_sprintf(ip_str, IPSTR, IP2STR(&(ipaddr->addr)));
    }
    lua_pushstring(L, ip_str);   // the ip para
  }
  // "enhanced" end

  lua_call(L, 2, 0);

end:
  if((pesp_conn->type == ESPCONN_TCP && pesp_conn->proto.tcp->remote_port == 0)
    || (pesp_conn->type == ESPCONN_UDP && pesp_conn->proto.udp->remote_port == 0) ){
    lua_gc(L, LUA_GCSTOP, 0);
    if(nud->self_ref != LUA_NOREF){
      luaL_unref(L, LUA_REGISTRYINDEX, nud->self_ref);
      nud->self_ref = LUA_NOREF; // unref this, and the net.socket userdata will delete it self
    }
    lua_gc(L, LUA_GCRESTART, 0);
  }
}

static void net_server_connected(void *arg) // for tcp only
{
  NODE_DBG("net_server_connected is called.\n");
  struct espconn *pesp_conn = arg;
  int i = 0;
  lnet_userdata *skt = NULL;
  if(pesp_conn == NULL)
    return;

#if 0
  char temp[20] = {0};
  c_sprintf(temp, IPSTR, IP2STR( &(pesp_conn->proto.tcp->remote_ip) ) );
  NODE_DBG("remote ");
  NODE_DBG(temp);
  NODE_DBG(":");
  NODE_DBG("%d",pesp_conn->proto.tcp->remote_port);
  NODE_DBG(" connected.\n");
#endif

  for(i=0;i<MAX_SOCKET;i++){
    if(socket[i] == LUA_NOREF)  // found empty slot
    {
      break;
    }
  }
  if(i>=MAX_SOCKET) // can't create more socket
  {
    NODE_ERR("MAX_SOCKET\n");
    pesp_conn->reverse = NULL;    // not accept this conn
    if(pesp_conn->proto.tcp->remote_port || pesp_conn->proto.tcp->local_port)
      espconn_disconnect(pesp_conn);
    return;
  }

  if(tcpserver_cb_connect_ref == LUA_NOREF)
    return;
  lua_State *L = lua_getstate();

  lua_rawgeti(L, LUA_REGISTRYINDEX, tcpserver_cb_connect_ref);  // get function
  // create a new client object
  skt = (lnet_userdata *)lua_newuserdata(L, sizeof(lnet_userdata));

  if(!skt){
    NODE_ERR("can't newudata\n");
    lua_pop(L, 1);
    return;
  }
  // set its metatable
  luaL_getmetatable(L, "net.socket");
  lua_setmetatable(L, -2);
  // pre-initialize it, in case of errors
  skt->self_ref = LUA_NOREF;
  lua_pushvalue(L, -1);  // copy the top of stack
  skt->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);    // ref to it self, for module api to find the userdata
  socket[i] = skt->self_ref;  // save to socket array
  socket_num++;
  skt->cb_connect_ref = LUA_NOREF;  // this socket already connected
  skt->cb_reconnect_ref = LUA_NOREF;
  skt->cb_disconnect_ref = LUA_NOREF;

  skt->cb_receive_ref = LUA_NOREF;
  skt->cb_send_ref = LUA_NOREF;
  skt->cb_dns_found_ref = LUA_NOREF;

#ifdef CLIENT_SSL_ENABLE
  skt->secure = 0;    // as a server SSL is not supported.
#endif

  skt->pesp_conn = pesp_conn;   // point to the espconn made by low level sdk
  pesp_conn->reverse = skt;   // let espcon carray the info of this userdata(net.socket)

  espconn_regist_recvcb(pesp_conn, net_socket_received);
  espconn_regist_sentcb(pesp_conn, net_socket_sent);
  espconn_regist_disconcb(pesp_conn, net_server_disconnected);
  espconn_regist_reconcb(pesp_conn, net_server_reconnected);

  // now socket[i] has the client ref, and stack top has the userdata
  lua_call(L, 1, 0);  // function(conn)
}

static void net_socket_connected(void *arg)
{
  NODE_DBG("net_socket_connected is called.\n");
  struct espconn *pesp_conn = arg;
  if(pesp_conn == NULL)
    return;
  lnet_userdata *nud = (lnet_userdata *)pesp_conn->reverse;
  if(nud == NULL)
    return;
  // can receive and send data, even if there is no connected callback in lua.
  espconn_regist_recvcb(pesp_conn, net_socket_received);
  espconn_regist_sentcb(pesp_conn, net_socket_sent);
  espconn_regist_disconcb(pesp_conn, net_socket_disconnected);

  if(nud->cb_connect_ref == LUA_NOREF)
    return;
  if(nud->self_ref == LUA_NOREF)
    return;
  lua_State *L = lua_getstate();
  lua_rawgeti(L, LUA_REGISTRYINDEX, nud->cb_connect_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, nud->self_ref);  // pass the userdata(client) to callback func in lua
  lua_call(L, 1, 0);
}

// Lua: s = net.create(type, secure/timeout, function(conn))
static int net_create( lua_State* L, const char* mt )
{
  NODE_DBG("net_create is called.\n");
  struct espconn *pesp_conn = NULL;
  lnet_userdata *nud, *temp = NULL;
  unsigned type;
#ifdef CLIENT_SSL_ENABLE
  unsigned secure = 0;
#endif
  uint8_t stack = 1;
  bool isserver = false;
  
  if (mt!=NULL && c_strcmp(mt, "net.server")==0)
    isserver = true;
  else if (mt!=NULL && c_strcmp(mt, "net.socket")==0)
    isserver = false;
  else
  {
    NODE_DBG("wrong metatable for net_create.\n");
    return 0;
  }

  type = luaL_checkinteger( L, stack );
  if ( type != ESPCONN_TCP && type != ESPCONN_UDP )
    return luaL_error( L, "wrong arg type" );
  stack++;
#ifdef CLIENT_SSL_ENABLE
  if(!isserver){
    if ( lua_isnumber(L, stack) )
    {
      secure = lua_tointeger(L, stack);
      stack++;
      if ( secure != 0 && secure != 1 ){
        return luaL_error( L, "wrong arg type" );
      }
    } else {
      secure = 0; // default to 0
    }
  }
#endif

  if(isserver && type == ESPCONN_TCP){
    if ( lua_isnumber(L, stack) )
    {
      unsigned to = lua_tointeger(L, stack);
      stack++;
      if ( to < 1 || to > 28800 ){
        return luaL_error( L, "wrong arg type" );
      }
      tcp_server_timeover = (uint16_t)to;
    } else {
      tcp_server_timeover = 30; // default to 30
    }
  }

  // create a object
  nud = (lnet_userdata *)lua_newuserdata(L, sizeof(lnet_userdata));
  // pre-initialize it, in case of errors
  nud->self_ref = LUA_NOREF;
  nud->cb_connect_ref = LUA_NOREF;
  nud->cb_reconnect_ref = LUA_NOREF;
  nud->cb_disconnect_ref = LUA_NOREF;
  nud->cb_receive_ref = LUA_NOREF;
  nud->cb_send_ref = LUA_NOREF;
  nud->cb_dns_found_ref = LUA_NOREF;
  nud->pesp_conn = NULL;
#ifdef CLIENT_SSL_ENABLE
  nud->secure = secure;
#endif

  // set its metatable
  luaL_getmetatable(L, mt);
  lua_setmetatable(L, -2);

  // create the espconn struct
  if(isserver && type==ESPCONN_TCP && pTcpServer){
    if(tcpserver_cb_connect_ref != LUA_NOREF){      // self_ref should be unref in close()
      lua_pop(L,1);
      return luaL_error(L, "only one tcp server allowed");
    }
    pesp_conn = nud->pesp_conn = pTcpServer;
  } else if(isserver && type==ESPCONN_UDP && pUdpServer){
    temp = (lnet_userdata *)pUdpServer->reverse;
    if(temp && temp->self_ref != LUA_NOREF){
      lua_pop(L,1);
      return luaL_error(L, "only one udp server allowed");
    }
    pesp_conn = nud->pesp_conn = pUdpServer;
  } else {
    pesp_conn = nud->pesp_conn = (struct espconn *)c_zalloc(sizeof(struct espconn));
    if(!pesp_conn)
      return luaL_error(L, "not enough memory");

    pesp_conn->proto.tcp = NULL;
    pesp_conn->proto.udp = NULL;
    pesp_conn->reverse = NULL;
    if( type==ESPCONN_TCP )
    {
      pesp_conn->proto.tcp = (esp_tcp *)c_zalloc(sizeof(esp_tcp));
      if(!pesp_conn->proto.tcp){
        c_free(pesp_conn);
        pesp_conn = nud->pesp_conn = NULL;
        return luaL_error(L, "not enough memory");
      }
      NODE_DBG("TCP server/socket is set.\n");
    }
    else if( type==ESPCONN_UDP )
    {
      pesp_conn->proto.udp = (esp_udp *)c_zalloc(sizeof(esp_udp));
      if(!pesp_conn->proto.udp){
        c_free(pesp_conn);
        pesp_conn = nud->pesp_conn = NULL;
        return luaL_error(L, "not enough memory");
      }
      NODE_DBG("UDP server/socket is set.\n");
    }
  }
  pesp_conn->type = type;
  pesp_conn->state = ESPCONN_NONE;
  // reverse is for the callback function
  pesp_conn->reverse = nud;

  if(isserver && type==ESPCONN_TCP && pTcpServer==NULL){
    pTcpServer = pesp_conn;
  } else if(isserver && type==ESPCONN_UDP && pUdpServer==NULL){
    pUdpServer = pesp_conn;
  }

  // if call back function is specified, call it with para userdata
  // luaL_checkanyfunction(L, 2);
  if (lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION){
    lua_pushvalue(L, stack);  // copy argument (func) to the top of stack
    lua_pushvalue(L, -2);  // copy the self_ref(userdata) to the top
    lua_call(L, 1, 0);
  }

  return 1; 
}

// static int net_close( lua_State* L, const char* mt );
// Lua: net.delete( socket/server )
// call close() first
// server: disconnect server, unref everything
// socket: unref everything
static int net_delete( lua_State* L, const char* mt )
{
  NODE_DBG("net_delete is called.\n");
  bool isserver = false;
  if (mt!=NULL && c_strcmp(mt, "net.server")==0)
    isserver = true;
  else if (mt!=NULL && c_strcmp(mt, "net.socket")==0)
    isserver = false;
  else
  {
    NODE_DBG("wrong metatable for net_delete.\n");
    return 0;
  }

  // net_close( L, mt );   // close it first

  lnet_userdata *nud = (lnet_userdata *)luaL_checkudata(L, 1, mt);
  luaL_argcheck(L, nud, 1, "Server/Socket expected");
  if(nud==NULL){
  	NODE_DBG("userdata is nil.\n");
  	return 0;
  }
  if(nud->pesp_conn){     // for client connected to tcp server, this should set NULL in disconnect cb
  	nud->pesp_conn->reverse = NULL;
    if(!isserver)   // socket is freed here
    {
      if(nud->pesp_conn->type == ESPCONN_UDP){
  	    if(nud->pesp_conn->proto.udp)
  	      c_free(nud->pesp_conn->proto.udp);
  	    nud->pesp_conn->proto.udp = NULL;
  	  } else if (nud->pesp_conn->type == ESPCONN_TCP) {
  	    if(nud->pesp_conn->proto.tcp)
  	      c_free(nud->pesp_conn->proto.tcp);
  	    nud->pesp_conn->proto.tcp = NULL;
  	  }
      c_free(nud->pesp_conn);
    }
    nud->pesp_conn = NULL;    // for socket, it will free this when disconnected
  }

  // free (unref) callback ref
  if(LUA_NOREF!=nud->cb_connect_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_connect_ref);
    nud->cb_connect_ref = LUA_NOREF;
  }
  if(LUA_NOREF!=nud->cb_reconnect_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_reconnect_ref);
    nud->cb_reconnect_ref = LUA_NOREF;
  }
  if(LUA_NOREF!=nud->cb_disconnect_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_disconnect_ref);
    nud->cb_disconnect_ref = LUA_NOREF;
  }
  if(LUA_NOREF!=nud->cb_receive_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_receive_ref);
    nud->cb_receive_ref = LUA_NOREF;
  }
  if(LUA_NOREF!=nud->cb_send_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_send_ref);
    nud->cb_send_ref = LUA_NOREF;
  }
  if(LUA_NOREF!=nud->cb_dns_found_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_dns_found_ref);
    nud->cb_dns_found_ref = LUA_NOREF;
  }
  lua_gc(L, LUA_GCSTOP, 0);
  if(LUA_NOREF!=nud->self_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, nud->self_ref);
    nud->self_ref = LUA_NOREF;
  }
  lua_gc(L, LUA_GCRESTART, 0);
  return 0;  
}

static void socket_connect(struct espconn *pesp_conn)
{
  if(pesp_conn == NULL)
    return;
  lnet_userdata *nud = (lnet_userdata *)pesp_conn->reverse;
  if(nud == NULL)
    return;

  if( pesp_conn->type == ESPCONN_TCP )
  {
#ifdef CLIENT_SSL_ENABLE
    if(nud->secure){
      espconn_secure_connect(pesp_conn);
    }
    else
#endif
    {
      espconn_connect(pesp_conn);
    }
  }
  else if (pesp_conn->type == ESPCONN_UDP)
  {
    espconn_create(pesp_conn);
  }
  NODE_DBG("socket_connect is called.\n");
}

static void socket_dns_found(const char *name, ip_addr_t *ipaddr, void *arg);
static int dns_reconn_count = 0;
static void socket_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
  NODE_DBG("socket_dns_found is called.\n");
  struct espconn *pesp_conn = arg;
  if(pesp_conn == NULL){
    NODE_DBG("pesp_conn null.\n");
    return;
  }
  lnet_userdata *nud = (lnet_userdata *)pesp_conn->reverse;
  if(nud == NULL)
    return;
  lua_State *L = lua_getstate();
  if(ipaddr == NULL)
  {
    dns_reconn_count++;
    if( dns_reconn_count >= 5 ){
      NODE_ERR( "DNS Fail!\n" );
      lua_gc(L, LUA_GCSTOP, 0);
      if(nud->self_ref != LUA_NOREF){
        luaL_unref(L, LUA_REGISTRYINDEX, nud->self_ref);
        nud->self_ref = LUA_NOREF; // unref this, and the net.socket userdata will delete it self
      }
      lua_gc(L, LUA_GCRESTART, 0);
      return;
    }
    NODE_ERR( "DNS retry %d!\n", dns_reconn_count );
    host_ip.addr = 0;
    espconn_gethostbyname(pesp_conn, name, &host_ip, socket_dns_found);
    return;
  }

  // ipaddr->addr is a uint32_t ip
  if(ipaddr->addr != 0)
  {
    dns_reconn_count = 0;
    if( pesp_conn->type == ESPCONN_TCP )
    {
      c_memcpy(pesp_conn->proto.tcp->remote_ip, &(ipaddr->addr), 4);
      NODE_DBG("TCP ip is set: ");
      NODE_DBG(IPSTR, IP2STR(&(ipaddr->addr)));
      NODE_DBG("\n");
    }
    else if (pesp_conn->type == ESPCONN_UDP)
    {
      c_memcpy(pesp_conn->proto.udp->remote_ip, &(ipaddr->addr), 4);
      NODE_DBG("UDP ip is set: ");
      NODE_DBG(IPSTR, IP2STR(&(ipaddr->addr)));
      NODE_DBG("\n");
    }
    socket_connect(pesp_conn);
  }
}

// Lua: server:listen( port, ip, function(con) )
// Lua: socket:connect( port, ip, function(con) )
static int net_start( lua_State* L, const char* mt )
{
  NODE_DBG("net_start is called.\n");
  struct espconn *pesp_conn = NULL;
  lnet_userdata *nud;
  unsigned port;
  size_t il;
  bool isserver = false;
  ip_addr_t ipaddr;
  const char *domain;
  uint8_t stack = 1;
  
  if (mt!=NULL && c_strcmp(mt, "net.server")==0)
    isserver = true;
  else if (mt!=NULL && c_strcmp(mt, "net.socket")==0)
    isserver = false;
  else
  {
    NODE_DBG("wrong metatable for net_start.\n");
    return 0;
  }
  nud = (lnet_userdata *)luaL_checkudata(L, stack, mt);
  luaL_argcheck(L, nud, stack, "Server/Socket expected");
  stack++;

  if(nud==NULL){
  	NODE_DBG("userdata is nil.\n");
  	return 0;
  }

  if(nud->pesp_conn == NULL){
    NODE_DBG("nud->pesp_conn is NULL.\n");
    return 0;
  }
  pesp_conn = nud->pesp_conn;
  port = luaL_checkinteger( L, stack );
  stack++;
  if( pesp_conn->type == ESPCONN_TCP )
  {
    if(isserver)
      pesp_conn->proto.tcp->local_port = port;
    else{
      pesp_conn->proto.tcp->remote_port = port;
      pesp_conn->proto.tcp->local_port = espconn_port();
    }
    NODE_DBG("TCP port is set: %d.\n", port);
  }
  else if (pesp_conn->type == ESPCONN_UDP)
  {
    if(isserver)
      pesp_conn->proto.udp->local_port = port;
    else{
      pesp_conn->proto.udp->remote_port = port;
      pesp_conn->proto.udp->local_port = espconn_port();
    }
    NODE_DBG("UDP port is set: %d.\n", port);
  }

  if( lua_isstring(L,stack) )   // deal with the domain string
  {
    domain = luaL_checklstring( L, stack, &il );
    stack++;
    if (domain == NULL)
    {
      if(isserver)
        domain = "0.0.0.0";
      else
        domain = "127.0.0.1";
    }
    ipaddr.addr = ipaddr_addr(domain);
    if( pesp_conn->type == ESPCONN_TCP )
    {
      if(isserver)
        c_memcpy(pesp_conn->proto.tcp->local_ip, &ipaddr.addr, 4);
      else
        c_memcpy(pesp_conn->proto.tcp->remote_ip, &ipaddr.addr, 4);
      NODE_DBG("TCP ip is set: ");
      NODE_DBG(IPSTR, IP2STR(&ipaddr.addr));
      NODE_DBG("\n");
    }
    else if (pesp_conn->type == ESPCONN_UDP)
    {
      if(isserver)
        c_memcpy(pesp_conn->proto.udp->local_ip, &ipaddr.addr, 4);
      else
        c_memcpy(pesp_conn->proto.udp->remote_ip, &ipaddr.addr, 4);
      NODE_DBG("UDP ip is set: ");
      NODE_DBG(IPSTR, IP2STR(&ipaddr.addr));
      NODE_DBG("\n");
    }
  }

  // call back function when a connection is obtained, tcp only
  if ( pesp_conn->type == ESPCONN_TCP ) {
    if (lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION){
      lua_pushvalue(L, stack);  // copy argument (func) to the top of stack
      if(isserver)    // for tcp server connected callback
      {
        if(tcpserver_cb_connect_ref != LUA_NOREF)
          luaL_unref(L, LUA_REGISTRYINDEX, tcpserver_cb_connect_ref);
        tcpserver_cb_connect_ref = luaL_ref(L, LUA_REGISTRYINDEX);
      } 
      else 
      {
        if(nud->cb_connect_ref != LUA_NOREF)
          luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_connect_ref);
        nud->cb_connect_ref = luaL_ref(L, LUA_REGISTRYINDEX);
      }
    }
  }

  if(!isserver || pesp_conn->type == ESPCONN_UDP){    // self_ref is only needed by socket userdata, or udp server
  	lua_pushvalue(L, 1);  // copy to the top of stack
    if(nud->self_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, nud->self_ref);
  	nud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  if( pesp_conn->type == ESPCONN_TCP )
  {
    if(isserver){   // no secure server support for now
      espconn_regist_connectcb(pesp_conn, net_server_connected);
      // tcp server, SSL is not supported
#ifdef CLIENT_SSL_ENABLE
      // if(nud->secure)
      //   espconn_secure_accept(pesp_conn);
      // else
#endif
        espconn_accept(pesp_conn);    // if it's a server, no need to dns.
        espconn_regist_time(pesp_conn, tcp_server_timeover, 0);
    }
    else{
      espconn_regist_connectcb(pesp_conn, net_socket_connected);
      espconn_regist_reconcb(pesp_conn, net_socket_reconnected);
#ifdef CLIENT_SSL_ENABLE
      if(nud->secure){
      	if(pesp_conn->proto.tcp->remote_port || pesp_conn->proto.tcp->local_port)
          espconn_secure_disconnect(pesp_conn);
        // espconn_secure_connect(pesp_conn);
      }
      else
#endif
      {
      	if(pesp_conn->proto.tcp->remote_port || pesp_conn->proto.tcp->local_port)
          espconn_disconnect(pesp_conn);
        // espconn_connect(pesp_conn);
      }
    }
  }
  else if (pesp_conn->type == ESPCONN_UDP)
  {
    espconn_regist_recvcb(pesp_conn, net_socket_received);
    espconn_regist_sentcb(pesp_conn, net_socket_sent);
  	if(pesp_conn->proto.tcp->remote_port || pesp_conn->proto.tcp->local_port)
    	espconn_delete(pesp_conn);
    if(isserver)
      espconn_create(pesp_conn);    // if it's a server, no need to dns.
  }

  if(!isserver){
    if((ipaddr.addr == IPADDR_NONE) && (c_memcmp(domain,"255.255.255.255",16) != 0))
    {
      host_ip.addr = 0;
      dns_reconn_count = 0;
      if(ESPCONN_OK == espconn_gethostbyname(pesp_conn, domain, &host_ip, socket_dns_found)){
        socket_dns_found(domain, &host_ip, pesp_conn);  // ip is returned in host_ip.
      }
    }
    else
    {
      socket_connect(pesp_conn);
    }
  }
  return 0;  
}

// Lua: server/socket:close()
// server disconnect everything, unref everything
// client disconnect and unref itself
static int net_close( lua_State* L, const char* mt )
{
  NODE_DBG("net_close is called.\n");
  bool isserver = false;
  int i = 0;
  lnet_userdata *nud = NULL, *skt = NULL;

  nud = (lnet_userdata *)luaL_checkudata(L, 1, mt);
  luaL_argcheck(L, nud, 1, "Server/Socket expected");
  if(nud == NULL)
    return 0;

  if(nud->pesp_conn == NULL)
    return 0;

  if (mt!=NULL && c_strcmp(mt, "net.server")==0)
    isserver = true;
  else if (mt!=NULL && c_strcmp(mt, "net.socket")==0)
    isserver = false;
  else
  {
    NODE_DBG("wrong metatable for net_close.\n");
    return 0;
  }

  if(isserver && nud->pesp_conn->type == ESPCONN_TCP && tcpserver_cb_connect_ref != LUA_NOREF){
    luaL_unref(L, LUA_REGISTRYINDEX, tcpserver_cb_connect_ref);
    tcpserver_cb_connect_ref = LUA_NOREF;
  }

  int n = lua_gettop(L);
  skt = nud;

  do{
    if(isserver && skt == NULL){
      if(socket[i] != LUA_NOREF){  // there is client socket exists
        lua_rawgeti(L, LUA_REGISTRYINDEX, socket[i]);    // get the referenced user_data to stack top
#if 0
        socket[i] = LUA_NOREF;
        socket_num--;
#endif  // do this in net_server_disconnected
        i++;
        if(lua_isuserdata(L,-1)){
          skt = lua_touserdata(L,-1);
        } else {
          lua_pop(L, 1);
          continue;
        }
      }else{
        // skip LUA_NOREF
        i++;
        continue;
      }
    }

    if(skt==NULL){
    	NODE_DBG("userdata is nil.\n");
    	continue;
    }

    if(skt->pesp_conn)    // disconnect the connection
    {
      if(skt->pesp_conn->type == ESPCONN_TCP)
      {
  #ifdef CLIENT_SSL_ENABLE
        if(skt->secure){
        	if(skt->pesp_conn->proto.tcp->remote_port || skt->pesp_conn->proto.tcp->local_port)
        	  espconn_secure_disconnect(skt->pesp_conn);
        }
        else
  #endif
        {
        	if(skt->pesp_conn->proto.tcp->remote_port || skt->pesp_conn->proto.tcp->local_port)
            espconn_disconnect(skt->pesp_conn);
        }
      }else if(skt->pesp_conn->type == ESPCONN_UDP)
      {
        if(skt->pesp_conn->proto.tcp->remote_port || skt->pesp_conn->proto.tcp->local_port)
          espconn_delete(skt->pesp_conn);

        // a udp server/socket unref it self here. not in disconnect.
        if(LUA_NOREF!=skt->self_ref){    // for a udp self_ref is NOREF
          luaL_unref(L, LUA_REGISTRYINDEX, skt->self_ref);
          skt->self_ref = LUA_NOREF;   // for a socket, now only var in lua is ref to the userdata
        }
      }
    }
#if 0
    // unref the self_ref
    if(LUA_NOREF!=skt->self_ref){    // for a server self_ref is NOREF
    	luaL_unref(L, LUA_REGISTRYINDEX, skt->self_ref);
    	skt->self_ref = LUA_NOREF;   // for a socket, now only var in lua is ref to the userdata
    }
#endif
    lua_settop(L, n);   // reset the stack top
    skt = NULL;
  } while( isserver && i<MAX_SOCKET);
#if 0
  // unref the self_ref, for both socket and server
  if(LUA_NOREF!=nud->self_ref){    // for a server self_ref is NOREF
    luaL_unref(L, LUA_REGISTRYINDEX, nud->self_ref);
    nud->self_ref = LUA_NOREF;   // now only var in lua is ref to the userdata
  }
#endif

  return 0;  
}

// Lua: socket/udpserver:on( "method", function(s) )
static int net_on( lua_State* L, const char* mt )
{
  NODE_DBG("net_on is called.\n");
  bool isserver = false;
  lnet_userdata *nud;
  size_t sl;
  
  nud = (lnet_userdata *)luaL_checkudata(L, 1, mt);
  luaL_argcheck(L, nud, 1, "Server/Socket expected");
  if(nud==NULL){
  	NODE_DBG("userdata is nil.\n");
  	return 0;
  }

  if (mt!=NULL && c_strcmp(mt, "net.server")==0)
    isserver = true;
  else if (mt!=NULL && c_strcmp(mt, "net.socket")==0)
    isserver = false;
  else
  {
    NODE_DBG("wrong metatable for net_on.\n");
    return 0;
  }

  const char *method = luaL_checklstring( L, 2, &sl );
  if (method == NULL)
    return luaL_error( L, "wrong arg type" );

  luaL_checkanyfunction(L, 3);
  lua_pushvalue(L, 3);  // copy argument (func) to the top of stack

  if(!isserver && nud->pesp_conn->type == ESPCONN_TCP && sl == 10 && c_strcmp(method, "connection") == 0){
    if(nud->cb_connect_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_connect_ref);
    nud->cb_connect_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }else if(!isserver && nud->pesp_conn->type == ESPCONN_TCP && sl == 12 && c_strcmp(method, "reconnection") == 0){
    if(nud->cb_reconnect_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_reconnect_ref);
    nud->cb_reconnect_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }else if(!isserver && nud->pesp_conn->type == ESPCONN_TCP && sl == 13 && c_strcmp(method, "disconnection") == 0){
    if(nud->cb_disconnect_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_disconnect_ref);
    nud->cb_disconnect_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }else if((!isserver || nud->pesp_conn->type == ESPCONN_UDP) && sl == 7 && c_strcmp(method, "receive") == 0){
    if(nud->cb_receive_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_receive_ref);
    nud->cb_receive_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }else if((!isserver || nud->pesp_conn->type == ESPCONN_UDP) && sl == 4 && c_strcmp(method, "sent") == 0){
    if(nud->cb_send_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_send_ref);
    nud->cb_send_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }else if(!isserver && nud->pesp_conn->type == ESPCONN_TCP && sl == 3 && c_strcmp(method, "dns") == 0){
    if(nud->cb_dns_found_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_dns_found_ref);
    nud->cb_dns_found_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }else{
  	lua_pop(L, 1);
    return luaL_error( L, "method not supported" );
  }

  return 0;  
}

// Lua: server/socket:send( string, function(sent) )
static int net_send( lua_State* L, const char* mt )
{
  // NODE_DBG("net_send is called.\n");
  bool isserver = false;
  struct espconn *pesp_conn = NULL;
  lnet_userdata *nud;
  size_t l;
  
  nud = (lnet_userdata *)luaL_checkudata(L, 1, mt);
  luaL_argcheck(L, nud, 1, "Server/Socket expected");
  if(nud==NULL){
  	NODE_DBG("userdata is nil.\n");
  	return 0;
  }

  if(nud->pesp_conn == NULL){
    NODE_DBG("nud->pesp_conn is NULL.\n");
    return 0;
  }
  pesp_conn = nud->pesp_conn;

  if (mt!=NULL && c_strcmp(mt, "net.server")==0)
    isserver = true;
  else if (mt!=NULL && c_strcmp(mt, "net.socket")==0)
    isserver = false;
  else
  {
    NODE_DBG("wrong metatable for net_send.\n");
    return 0;
  }

  if(isserver && nud->pesp_conn->type == ESPCONN_TCP){
    return luaL_error( L, "tcp server send not supported" );
  }

#if 0
  char temp[20] = {0};
  c_sprintf(temp, IPSTR, IP2STR( &(pesp_conn->proto.tcp->remote_ip) ) );
  NODE_DBG("remote ");
  NODE_DBG(temp);
  NODE_DBG(":");
  NODE_DBG("%d",pesp_conn->proto.tcp->remote_port);
  NODE_DBG(" sending data.\n");
#endif

  const char *payload = luaL_checklstring( L, 2, &l );
  if (l>1460 || payload == NULL)
    return luaL_error( L, "need <1460 payload" );

  if (lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TLIGHTFUNCTION){
    lua_pushvalue(L, 3);  // copy argument (func) to the top of stack
    if(nud->cb_send_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_send_ref);
    nud->cb_send_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  // SDK 1.4.0 changed behaviour, for UDP server need to look up remote ip/port
  if (isserver && pesp_conn->type == ESPCONN_UDP)
  {
    remot_info *pr = 0;
    if (espconn_get_connection_info (pesp_conn, &pr, 0) != ESPCONN_OK)
      return luaL_error (L, "remote ip/port unavailable");
    pesp_conn->proto.udp->remote_port = pr->remote_port;
    os_memmove (pesp_conn->proto.udp->remote_ip, pr->remote_ip, 4);
    // The remot_info apparently should *not* be os_free()d, fyi
  }
#ifdef CLIENT_SSL_ENABLE
  if(nud->secure)
    espconn_secure_sent(pesp_conn, (unsigned char *)payload, l);
  else
#endif
    espconn_sent(pesp_conn, (unsigned char *)payload, l);

  return 0;  
}

// Lua: socket:dns( string, function(socket, ip) )
static int net_dns( lua_State* L, const char* mt )
{
  NODE_DBG("net_dns is called.\n");
  bool isserver = false;
  struct espconn *pesp_conn = NULL;
  lnet_userdata *nud;
  size_t l;
  
  nud = (lnet_userdata *)luaL_checkudata(L, 1, mt);
  luaL_argcheck(L, nud, 1, "Server/Socket expected");
  if(nud==NULL){
    NODE_DBG("userdata is nil.\n");
    return 0;
  }

  if (mt!=NULL && c_strcmp(mt, "net.server")==0)
    isserver = true;
  else if (mt!=NULL && c_strcmp(mt, "net.socket")==0)
    isserver = false;
  else
  {
    NODE_DBG("wrong metatable for net_send.\n");
    return 0;
  }

  if(nud->pesp_conn == NULL){
    NODE_DBG("nud->pesp_conn is NULL.\n");
    return 0;
  }
  pesp_conn = nud->pesp_conn;

  if(!isserver || pesp_conn->type == ESPCONN_UDP){    // self_ref is only needed by socket userdata, or udp server
    lua_pushvalue(L, 1);  // copy to the top of stack
    if(nud->self_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, nud->self_ref);
    nud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  const char *domain = luaL_checklstring( L, 2, &l );
  if (l>128 || domain == NULL)
    return luaL_error( L, "need <128 domain" );

  if (lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TLIGHTFUNCTION){
    lua_pushvalue(L, 3);  // copy argument (func) to the top of stack
    if(nud->cb_dns_found_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_dns_found_ref);
    nud->cb_dns_found_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  host_ip.addr = 0;
  if(ESPCONN_OK == espconn_gethostbyname(pesp_conn, domain, &host_ip, net_dns_found))
    net_dns_found(domain, &host_ip, pesp_conn);  // ip is returned in host_ip.


  return 0;  
}

// Lua: net.dns.resolve( domain, function(ip) )
static int net_dns_static( lua_State* L )
{
  const char *mt = "net.socket";
  if (!lua_isstring( L, 1 ))
    return luaL_error( L, "wrong parameter type (domain)" );
  
  int rfunc = LUA_NOREF; //save reference to func
  if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION){
    rfunc = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  int rdom = luaL_ref(L, LUA_REGISTRYINDEX); //save reference to domain

  lua_settop(L,0); //empty stack
  lua_getfield(L, LUA_GLOBALSINDEX, "net");
  lua_getfield(L, -1, "createConnection");
  lua_remove(L, -2); //remove "net" from stack
  lua_pushinteger(L, UDP); // we are going to create a new dummy UDP socket
  lua_call(L,1,1);// after this the stack should have a socket

  lua_rawgeti(L, LUA_REGISTRYINDEX, rdom);    // load domain back to the stack
  lua_rawgeti(L, LUA_REGISTRYINDEX, rfunc);    // load the callback function back to the stack

  luaL_unref(L, LUA_REGISTRYINDEX, rdom); //free reference
  luaL_unref(L, LUA_REGISTRYINDEX, rfunc); //free reference

  bool isserver = false;
  struct espconn *pesp_conn = NULL;
  lnet_userdata *nud;
  size_t l;
  
  nud = (lnet_userdata *)luaL_checkudata(L, 1, mt);
  luaL_argcheck(L, nud, 1, "Server/Socket expected");
  if(nud==NULL){
    NODE_DBG("userdata is nil.\n");
    return 0;
  }
  if(nud->pesp_conn == NULL){
    NODE_DBG("nud->pesp_conn is NULL.\n");
    return 0;
  }
  pesp_conn = nud->pesp_conn;

  lua_pushvalue(L, 1);  // copy to the top of stack
  if(nud->self_ref != LUA_NOREF)
    luaL_unref(L, LUA_REGISTRYINDEX, nud->self_ref);
  nud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  const char *domain = luaL_checklstring( L, 2, &l );
  if (l>128 || domain == NULL)
    return luaL_error( L, "need <128 domain" );

  if (lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TLIGHTFUNCTION){
    lua_pushvalue(L, 3);  // copy argument (func) to the top of stack
    if(nud->cb_dns_found_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, nud->cb_dns_found_ref);
    nud->cb_dns_found_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  host_ip.addr = 0;
  if(ESPCONN_OK == espconn_gethostbyname(pesp_conn, domain, &host_ip, net_dns_found))
    net_dns_found(domain, &host_ip, pesp_conn);  // ip is returned in host_ip.


  return 0;
}

// Lua: s = net.createServer(type, function(server))
static int net_createServer( lua_State* L )
{
  const char *mt = "net.server";
  return net_create(L, mt);
}

// Lua: server:delete()
static int net_server_delete( lua_State* L )
{
  const char *mt = "net.server";
  return net_delete(L, mt);
}

// Lua: server:listen( port, ip )
static int net_server_listen( lua_State* L )
{
  const char *mt = "net.server";
  return net_start(L, mt);
}

// Lua: server:close()
static int net_server_close( lua_State* L )
{
  const char *mt = "net.server";
  return net_close(L, mt);
}

// Lua: udpserver:on( "method", function(udpserver) )
static int net_udpserver_on( lua_State* L )
{
  const char *mt = "net.server";
  return net_on(L, mt);
}

// Lua: udpserver:send(string, function() )
static int net_udpserver_send( lua_State* L )
{
  const char *mt = "net.server";
  return net_send(L, mt);;
}

// Lua: s = net.createConnection(type, function(conn))
static int net_createConnection( lua_State* L )
{
  const char *mt = "net.socket";
  return net_create(L, mt);
}

// Lua: socket:delete()
static int net_socket_delete( lua_State* L )
{
  const char *mt = "net.socket";
  return net_delete(L, mt);
}

// Lua: socket:connect( port, ip )
static int net_socket_connect( lua_State* L )
{
  const char *mt = "net.socket";
  return net_start(L, mt);
}

// Lua: socket:close()
static int net_socket_close( lua_State* L )
{
  const char *mt = "net.socket";
  return net_close(L, mt);
}

// Lua: socket:on( "method", function(socket) )
static int net_socket_on( lua_State* L )
{
  const char *mt = "net.socket";
  return net_on(L, mt);
}

// Lua: socket:send( string, function() )
static int net_socket_send( lua_State* L )
{
  const char *mt = "net.socket";
  return net_send(L, mt);
}

static int net_socket_hold( lua_State* L )
{
  const char *mt = "net.socket";
  struct espconn *pesp_conn = NULL;
  lnet_userdata *nud;
  size_t l;

  nud = (lnet_userdata *)luaL_checkudata(L, 1, mt);
  luaL_argcheck(L, nud, 1, "Server/Socket expected");
  if(nud==NULL){
    NODE_DBG("userdata is nil.\n");
    return 0;
  }

  if(nud->pesp_conn == NULL){
    NODE_DBG("nud->pesp_conn is NULL.\n");
    return 0;
  }
  pesp_conn = nud->pesp_conn;
  espconn_recv_hold(pesp_conn);

  return 0;
}

static int net_socket_unhold( lua_State* L )
{
  const char *mt = "net.socket";
  struct espconn *pesp_conn = NULL;
  lnet_userdata *nud;
  size_t l;

  nud = (lnet_userdata *)luaL_checkudata(L, 1, mt);
  luaL_argcheck(L, nud, 1, "Server/Socket expected");
  if(nud==NULL){
    NODE_DBG("userdata is nil.\n");
    return 0;
  }

  if(nud->pesp_conn == NULL){
    NODE_DBG("nud->pesp_conn is NULL.\n");
    return 0;
  }
  pesp_conn = nud->pesp_conn;
  espconn_recv_unhold(pesp_conn);

  return 0;
}

// Lua: ip,port = sk:getpeer()
static int net_socket_getpeer( lua_State* L )
{
  lnet_userdata *nud;
  const char *mt = "net.socket";
  nud = (lnet_userdata *)luaL_checkudata(L, 1, mt);
  luaL_argcheck(L, nud, 1, "Server/Socket expected");

  if(nud!=NULL && nud->pesp_conn!=NULL ){
      char temp[20] = {0};
      c_sprintf(temp, IPSTR, IP2STR( &(nud->pesp_conn->proto.tcp->remote_ip) ) );
      if ( nud->pesp_conn->proto.tcp->remote_port != 0 ) {
        lua_pushstring( L, temp );
        lua_pushinteger( L, nud->pesp_conn->proto.tcp->remote_port );
      } else {
        lua_pushnil( L );
        lua_pushnil( L );
      }
  } else {
      lua_pushnil( L );
      lua_pushnil( L );
  }
  return 2;
}

// Lua: socket:dns( string, function(ip) )
static int net_socket_dns( lua_State* L )
{
  const char *mt = "net.socket";
  return net_dns(L, mt);
}

static int net_multicastJoinLeave( lua_State *L, int join)
{
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
		  espconn_igmp_join(&if_addr, &multicast_addr);
	  }
	  else
	  {
		  espconn_igmp_leave(&if_addr, &multicast_addr);
	  }
	  return 0;
}
// Lua: net.multicastJoin(ifip, multicastip)
// if ifip "" or "any" all interfaces are affected
static int net_multicastJoin( lua_State* L )
{
	return net_multicastJoinLeave(L,1);
}

// Lua: net.multicastLeave(ifip, multicastip)
// if ifip "" or "any" all interfaces are affected
static int net_multicastLeave( lua_State* L )
{
	return net_multicastJoinLeave(L,0);
}

// Returns NULL on success, error message otherwise
static const char *append_pem_blob(const char *pem, const char *type, uint8_t **buffer_p, uint8_t *buffer_limit, const char *name) {
  char unb64[256];
  memset(unb64, 0xff, sizeof(unb64));
  int i;
  for (i = 0; i < 64; i++) {
    unb64["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
  }

  if (!pem) {
    return "No PEM blob";
  }

  // Scan for -----BEGIN CERT
  pem = strstr(pem, "-----BEGIN ");
  if (!pem) {
    return "No PEM header";
  }

  if (strncmp(pem + 11, type, strlen(type))) {
    return "Wrong PEM type";
  }

  pem = strchr(pem, '\n');
  if (!pem) {
    return "Incorrect PEM format";
  }
  //
  // Base64 encoded data starts here
  // Get all the base64 data into a single buffer....
  // We will use the back end of the buffer....
  //

  uint8_t *buffer = *buffer_p;

  uint8_t *dest = buffer + 32 + 2;  // Leave space for name and length
  int bitcount = 0;
  int accumulator = 0;
  for (; *pem && dest < buffer_limit; pem++) {
    int val = unb64[*(uint8_t*) pem];
    if (val & 0xC0) {
      // not a base64 character
      if (isspace(*(uint8_t*) pem)) {
	continue;
      }
      if (*pem == '=') {
	// just ignore -- at the end
	bitcount = 0;
	continue;
      }
      if (*pem == '-') {
	break;
      }
      return "Invalid character in PEM";
    } else {
      bitcount += 6;
      accumulator = (accumulator << 6) + val;
      if (bitcount >= 8) {
	bitcount -= 8;
	*dest++ = accumulator >> bitcount;
      }
    }
  }
  if (dest >= buffer_limit || strncmp(pem, "-----END ", 9) || strncmp(pem + 9, type, strlen(type)) || bitcount) {
    return "Invalid PEM format data";
  }
  size_t len = dest - (buffer + 32 + 2);

  memset(buffer, 0, 32);
  strcpy(buffer, name);
  buffer[32] = len & 0xff;
  buffer[33] = (len >> 8) & 0xff;
  *buffer_p = dest;
  return NULL;
}

static const char *fill_page_with_pem(lua_State *L, const unsigned char *flash_memory, int flash_offset, const char **types, const char **names) 
{
  uint8_t  *buffer = luaM_malloc(L, INTERNAL_FLASH_SECTOR_SIZE);
  uint8_t  *buffer_base = buffer;
  uint8_t  *buffer_limit = buffer + INTERNAL_FLASH_SECTOR_SIZE;

  int argno;

  for (argno = 1; argno <= lua_gettop(L) && types[argno - 1]; argno++) {
    const char *pem = lua_tostring(L, argno);

    const char *error = append_pem_blob(pem, types[argno - 1], &buffer, buffer_limit, names[argno - 1]);
    if (error) {
      luaM_free(L, buffer_base);
      return error;
    }
  }

  memset(buffer, 0xff, buffer_limit - buffer);

  // Lets see if it matches what is already there....
  if (c_memcmp(buffer_base, flash_memory, INTERNAL_FLASH_SECTOR_SIZE) != 0) {
    // Starts being dangerous
    if (platform_flash_erase_sector(flash_offset / INTERNAL_FLASH_SECTOR_SIZE) != PLATFORM_OK) {
      luaM_free(L, buffer_base);
      return "Failed to erase sector";
    }
    if (platform_s_flash_write(buffer_base, flash_offset, INTERNAL_FLASH_SECTOR_SIZE) != INTERNAL_FLASH_SECTOR_SIZE) {
      luaM_free(L, buffer_base);
      return "Failed to write sector";
    }
    // ends being dangerous
  }

  luaM_free(L, buffer_base);

  return NULL;
}

// Lua: net.cert.auth(true / false | PEM data [, PEM data] )
static int net_cert_auth(lua_State *L)
{
  int enable;

  uint32_t flash_offset = platform_flash_mapped2phys((uint32_t) &net_client_cert_area[0]);
  if ((flash_offset & 0xfff) || flash_offset > 0xff000 || INTERNAL_FLASH_SECTOR_SIZE != 0x1000) {
    // THis should never happen
    return luaL_error( L, "bad offset" );
  }

  if (lua_type(L, 1) == LUA_TSTRING) {
    const char *types[3] = { "CERTIFICATE", "RSA PRIVATE KEY", NULL };
    const char *names[2] = { "certificate", "private_key" };
    const char *error = fill_page_with_pem(L, &net_client_cert_area[0], flash_offset, types, names);
    if (error) {
      return luaL_error(L, error);
    }

    enable = 1;
  } else {
    enable = lua_toboolean(L, 1);
  }

  bool rc;

  if (enable) {
    // See if there is a cert there
    if (net_client_cert_area[0] == 0x00 || net_client_cert_area[0] == 0xff) {
      return luaL_error( L, "no certificates found" );
    }
    rc = espconn_secure_cert_req_enable(1, flash_offset / INTERNAL_FLASH_SECTOR_SIZE);
  } else {
    rc = espconn_secure_cert_req_disable(1);
  }

  lua_pushboolean(L, rc);
  return 1;
}

// Lua: net.cert.verify(true / false | PEM data [, PEM data] )
static int net_cert_verify(lua_State *L)
{
  int enable;

  uint32_t flash_offset = platform_flash_mapped2phys((uint32_t) &net_server_cert_area[0]);
  if ((flash_offset & 0xfff) || flash_offset > 0xff000 || INTERNAL_FLASH_SECTOR_SIZE != 0x1000) {
    // THis should never happen
    return luaL_error( L, "bad offset" );
  }

  if (lua_type(L, 1) == LUA_TSTRING) {
    const char *types[2] = { "CERTIFICATE", NULL };
    const char *names[1] = { "certificate" };

    const char *error = fill_page_with_pem(L, &net_server_cert_area[0], flash_offset, types, names);
    if (error) {
      return luaL_error(L, error);
    }

    enable = 1;
  } else {
    enable = lua_toboolean(L, 1);
  }

  bool rc;

  if (enable) {
    // See if there is a cert there
    if (net_server_cert_area[0] == 0x00 || net_server_cert_area[0] == 0xff) {
      return luaL_error( L, "no certificates found" );
    }
    rc = espconn_secure_ca_enable(1, flash_offset / INTERNAL_FLASH_SECTOR_SIZE);
  } else {
    rc = espconn_secure_ca_disable(1);
  }

  lua_pushboolean(L, rc);
  return 1;
}

// Lua: s = net.dns.setdnsserver(ip_addr, [index])
static int net_setdnsserver( lua_State* L )
{
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
static int net_getdnsserver( lua_State* L )
{
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
    c_sprintf(temp, IPSTR, IP2STR( &ipaddr.addr ) );
    lua_pushstring( L, temp );
  }

  return 1;
}

#if 0
static int net_array_index( lua_State* L )
{
  char** parray = luaL_checkudata(L, 1, "net.array");
  int index = luaL_checkint(L, 2);
  lua_pushnumber(L, (*parray)[index-1]);
  return 1;
}

static int net_array_newindex( lua_State* L )
{
  char** parray = luaL_checkudata(L, 1, "net.array");
  int index = luaL_checkint(L, 2);
  int value = luaL_checkint(L, 3);
  (*parray)[index-1] = value;
  return 0;
}

// expose an array to lua, by storing it in a userdata with the array metatable
static int expose_array(lua_State* L, char *array, unsigned short len) {
  char** parray = lua_newuserdata(L, len);
  *parray = array;
  luaL_getmetatable(L, "net.array");
  lua_setmetatable(L, -2);
  return 1;
}
#endif

// Module function map
static const LUA_REG_TYPE net_server_map[] = {
  { LSTRKEY( "listen" ),  LFUNCVAL( net_server_listen ) },
  { LSTRKEY( "close" ),   LFUNCVAL( net_server_close ) },
  { LSTRKEY( "on" ),      LFUNCVAL( net_udpserver_on ) },
  { LSTRKEY( "send" ),    LFUNCVAL( net_udpserver_send ) },
//{ LSTRKEY( "delete" ),  LFUNCVAL( net_server_delete ) },
  { LSTRKEY( "__gc" ),    LFUNCVAL( net_server_delete ) },
  { LSTRKEY( "__index" ), LROVAL( net_server_map ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE net_socket_map[] = {
  { LSTRKEY( "connect" ), LFUNCVAL( net_socket_connect ) },
  { LSTRKEY( "close" ),   LFUNCVAL( net_socket_close ) },
  { LSTRKEY( "on" ),      LFUNCVAL( net_socket_on ) },
  { LSTRKEY( "send" ),    LFUNCVAL( net_socket_send ) },
  { LSTRKEY( "hold" ),    LFUNCVAL( net_socket_hold ) },
  { LSTRKEY( "unhold" ),  LFUNCVAL( net_socket_unhold ) },
  { LSTRKEY( "dns" ),     LFUNCVAL( net_socket_dns ) },
  { LSTRKEY( "getpeer" ), LFUNCVAL( net_socket_getpeer ) },
//{ LSTRKEY( "delete" ),  LFUNCVAL( net_socket_delete ) },
  { LSTRKEY( "__gc" ),    LFUNCVAL( net_socket_delete ) },
  { LSTRKEY( "__index" ), LROVAL( net_socket_map ) },
  { LNILKEY, LNILVAL }
};
#if 0
static const LUA_REG_TYPE net_array_map[] = {
  { LSTRKEY( "__index" ),    LFUNCVAL( net_array_index ) },
  { LSTRKEY( "__newindex" ), LFUNCVAL( net_array_newindex ) },
  { LNILKEY, LNILVAL }
};
#endif

static const LUA_REG_TYPE net_cert_map[] = {
  { LSTRKEY( "verify" ), 	LFUNCVAL( net_cert_verify ) },  
#ifdef CLIENT_SSL_CERT_AUTH_ENABLE
  { LSTRKEY( "auth" ),		LFUNCVAL( net_cert_auth ) }, 
#endif
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
  { LSTRKEY( "multicastJoin"),     LFUNCVAL( net_multicastJoin ) },
  { LSTRKEY( "multicastLeave"),    LFUNCVAL( net_multicastLeave ) },
  { LSTRKEY( "dns" ),              LROVAL( net_dns_map ) },
#ifdef CLIENT_SSL_ENABLE
  { LSTRKEY( "cert" ),             LROVAL(net_cert_map) },
#endif
  { LSTRKEY( "TCP" ),              LNUMVAL( TCP ) },
  { LSTRKEY( "UDP" ),              LNUMVAL( UDP ) },
  { LSTRKEY( "__metatable" ),      LROVAL( net_map ) },
  { LNILKEY, LNILVAL }
};

int luaopen_net( lua_State *L ) {
  int i;
  for(i=0;i<MAX_SOCKET;i++)
  {
    socket[i] = LUA_NOREF;
  }

  luaL_rometatable(L, "net.server", (void *)net_server_map);  // create metatable for net.server
  luaL_rometatable(L, "net.socket", (void *)net_socket_map);  // create metatable for net.socket
  #if 0
  luaL_rometatable(L, "net.array", (void *)net_array_map);    // create metatable for net.array
  #endif

  return 0;
}

NODEMCU_MODULE(NET, "net", net_map, luaopen_net);
