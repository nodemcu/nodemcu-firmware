// Module for coapwork

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "c_string.h"
#include "c_stdlib.h"

#include "c_types.h"
#include "mem.h"
#include "lwip/ip_addr.h"
#include "espconn.h"
#include "driver/uart.h"

#include "coap.h"
#include "uri.h"
#include "node.h"
#include "coap_timer.h"
#include "coap_io.h"
#include "coap_server.h"

coap_queue_t *gQueue = NULL;

typedef struct lcoap_userdata
{
  struct espconn *pesp_conn;
  int self_ref;
}lcoap_userdata;

static void coap_received(void *arg, char *pdata, unsigned short len)
{
  NODE_DBG("coap_received is called.\n");
  struct espconn *pesp_conn = arg;
  lcoap_userdata *cud = (lcoap_userdata *)pesp_conn->reverse;

  // static uint8_t buf[MAX_MESSAGE_SIZE+1] = {0}; // +1 for string '\0'
  uint8_t buf[MAX_MESSAGE_SIZE+1] = {0}; // +1 for string '\0'
  c_memset(buf, 0, sizeof(buf)); // wipe prev data

  if (len > MAX_MESSAGE_SIZE) {
    NODE_DBG("Request Entity Too Large.\n"); // NOTE: should response 4.13 to client...
    return;
  }
  // c_memcpy(buf, pdata, len);

  size_t rsplen = coap_server_respond(pdata, len, buf, MAX_MESSAGE_SIZE+1);

  // SDK 1.4.0 changed behaviour, for UDP server need to look up remote ip/port
  remot_info *pr = 0;
  if (espconn_get_connection_info (pesp_conn, &pr, 0) != ESPCONN_OK)
    return;
  pesp_conn->proto.udp->remote_port = pr->remote_port;
  os_memmove (pesp_conn->proto.udp->remote_ip, pr->remote_ip, 4);
  // The remot_info apparently should *not* be os_free()d, fyi

  espconn_sent(pesp_conn, (unsigned char *)buf, rsplen);

  // c_memset(buf, 0, sizeof(buf));
}

static void coap_sent(void *arg)
{
  NODE_DBG("coap_sent is called.\n");
}

// Lua: s = coap.create(function(conn))
static int coap_create( lua_State* L, const char* mt )
{
  struct espconn *pesp_conn = NULL;
  lcoap_userdata *cud;
  unsigned type;
  int stack = 1;

  // create a object
  cud = (lcoap_userdata *)lua_newuserdata(L, sizeof(lcoap_userdata));
  // pre-initialize it, in case of errors
  cud->self_ref = LUA_NOREF;
  cud->pesp_conn = NULL;

  // set its metatable
  luaL_getmetatable(L, mt);
  lua_setmetatable(L, -2);

  // create the espconn struct
  pesp_conn = (struct espconn *)c_zalloc(sizeof(struct espconn));
  if(!pesp_conn)
    return luaL_error(L, "not enough memory");

  cud->pesp_conn = pesp_conn;

  pesp_conn->type = ESPCONN_UDP;
  pesp_conn->proto.tcp = NULL;
  pesp_conn->proto.udp = NULL;

  pesp_conn->proto.udp = (esp_udp *)c_zalloc(sizeof(esp_udp));
  if(!pesp_conn->proto.udp){
    c_free(pesp_conn);
    cud->pesp_conn = pesp_conn = NULL;
    return luaL_error(L, "not enough memory");
  }
  pesp_conn->state = ESPCONN_NONE;
  NODE_DBG("UDP server/client is set.\n");

  pesp_conn->reverse = cud;

  NODE_DBG("coap_create is called.\n");
  return 1;  
}

// Lua: server:delete()
static int coap_delete( lua_State* L, const char* mt )
{
  struct espconn *pesp_conn = NULL;
  lcoap_userdata *cud;

  cud = (lcoap_userdata *)luaL_checkudata(L, 1, mt);
  luaL_argcheck(L, cud, 1, "Server/Client expected");
  if(cud==NULL){
    NODE_DBG("userdata is nil.\n");
    return 0;
  }

  // free (unref) callback ref
  if(LUA_NOREF!=cud->self_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, cud->self_ref);
    cud->self_ref = LUA_NOREF;
  }

  if(cud->pesp_conn)
  {
    if(cud->pesp_conn->proto.udp->remote_port || cud->pesp_conn->proto.udp->local_port)
      espconn_delete(cud->pesp_conn);
    c_free(cud->pesp_conn->proto.udp);
    cud->pesp_conn->proto.udp = NULL;
    c_free(cud->pesp_conn);
    cud->pesp_conn = NULL;
  }

  NODE_DBG("coap_delete is called.\n");
  return 0;  
}

// Lua: server:listen( port, ip )
static int coap_start( lua_State* L, const char* mt )
{
  struct espconn *pesp_conn = NULL;
  lcoap_userdata *cud;
  unsigned port;
  size_t il;
  ip_addr_t ipaddr;

  cud = (lcoap_userdata *)luaL_checkudata(L, 1, mt);
  luaL_argcheck(L, cud, 1, "Server/Client expected");
  if(cud==NULL){
    NODE_DBG("userdata is nil.\n");
    return 0;
  }

  pesp_conn = cud->pesp_conn;
  port = luaL_checkinteger( L, 2 );
  pesp_conn->proto.udp->local_port = port;
  NODE_DBG("UDP port is set: %d.\n", port);

  if( lua_isstring(L,3) )   // deal with the ip string
  {
    const char *ip = luaL_checklstring( L, 3, &il );
    if (ip == NULL)
    {
      ip = "0.0.0.0";
    }
    ipaddr.addr = ipaddr_addr(ip);
    c_memcpy(pesp_conn->proto.udp->local_ip, &ipaddr.addr, 4);
    NODE_DBG("UDP ip is set: ");
    NODE_DBG(IPSTR, IP2STR(&ipaddr.addr));
    NODE_DBG("\n");
  }

  if(LUA_NOREF==cud->self_ref){
    lua_pushvalue(L, 1);  // copy to the top of stack
    cud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  espconn_regist_recvcb(pesp_conn, coap_received);
  espconn_regist_sentcb(pesp_conn, coap_sent);
  espconn_create(pesp_conn);

  NODE_DBG("Coap Server started on port: %d\n", port);
  NODE_DBG("coap_start is called.\n");
  return 0;  
}

// Lua: server:close()
static int coap_close( lua_State* L, const char* mt )
{
  struct espconn *pesp_conn = NULL;
  lcoap_userdata *cud;

  cud = (lcoap_userdata *)luaL_checkudata(L, 1, mt);
  luaL_argcheck(L, cud, 1, "Server/Client expected");
  if(cud==NULL){
    NODE_DBG("userdata is nil.\n");
    return 0;
  }

  if(cud->pesp_conn)
  {
    if(cud->pesp_conn->proto.udp->remote_port || cud->pesp_conn->proto.udp->local_port)
      espconn_delete(cud->pesp_conn);
  }

  if(LUA_NOREF!=cud->self_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, cud->self_ref);
    cud->self_ref = LUA_NOREF;
  }

  NODE_DBG("coap_close is called.\n");
  return 0;  
}

// Lua: server/client:on( "method", function(s) )
static int coap_on( lua_State* L, const char* mt )
{
  NODE_DBG("coap_on is called.\n");
  return 0;  
}

static void coap_response_handler(void *arg, char *pdata, unsigned short len)
{
  NODE_DBG("coap_response_handler is called.\n");
  struct espconn *pesp_conn = arg;

  coap_packet_t pkt;
  pkt.content.p = NULL;
  pkt.content.len = 0;
  // static uint8_t buf[MAX_MESSAGE_SIZE+1] = {0}; // +1 for string '\0'
  uint8_t buf[MAX_MESSAGE_SIZE+1] = {0}; // +1 for string '\0'
  c_memset(buf, 0, sizeof(buf)); // wipe prev data

  int rc;
  if( len > MAX_MESSAGE_SIZE )
  {
    NODE_DBG("Request Entity Too Large.\n"); // NOTE: should response 4.13 to client...
    return;
  }
  c_memcpy(buf, pdata, len);

  if (0 != (rc = coap_parse(&pkt, buf, len))){
    NODE_DBG("Bad packet rc=%d\n", rc);
  }
  else
  {
#ifdef COAP_DEBUG
    coap_dumpPacket(&pkt);
#endif
    /* check if this is a response to our original request */
    if (!check_token(&pkt)) {
      /* drop if this was just some message, or send RST in case of notification */
      if (pkt.hdr.t == COAP_TYPE_CON || pkt.hdr.t == COAP_TYPE_NONCON){
        // coap_send_rst(pkt);  // send RST response
        // or, just ignore it.
      }
      goto end;
    }

    if (pkt.hdr.t == COAP_TYPE_RESET) {
      NODE_DBG("got RST\n");
      goto end;
    }

    uint32_t ip = 0, port = 0;
    coap_tid_t id = COAP_INVALID_TID;

    c_memcpy(&ip, pesp_conn->proto.udp->remote_ip, sizeof(ip));
    port = pesp_conn->proto.udp->remote_port;

    coap_transaction_id(ip, port, &pkt, &id);

    /* transaction done, remove the node from queue */
    // stop timer
    coap_timer_stop();
    // remove the node
    coap_remove_node(&gQueue, id);
    // calculate time elapsed
    coap_timer_update(&gQueue);
    coap_timer_start(&gQueue);

    if (COAP_RESPONSE_CLASS(pkt.hdr.code) == 2)
    {
      /* There is no block option set, just read the data and we are done. */
      NODE_DBG("%d.%02d\t", (pkt.hdr.code >> 5), pkt.hdr.code & 0x1F);
      NODE_DBG((char *)pkt.payload.p);
    }
    else if (COAP_RESPONSE_CLASS(pkt.hdr.code) >= 4)
    {
      NODE_DBG("%d.%02d\t", (pkt.hdr.code >> 5), pkt.hdr.code & 0x1F);
      NODE_DBG((char *)pkt.payload.p);
    }
  }

end:
  if(!gQueue){ // if there is no node pending in the queue, disconnect from host.
    if(pesp_conn->proto.udp->remote_port || pesp_conn->proto.udp->local_port)
      espconn_delete(pesp_conn);
  }
  // c_memset(buf, 0, sizeof(buf));
}

// Lua: client:request( [CON], uri, [payload] )
static int coap_request( lua_State* L, coap_method_t m )
{
  struct espconn *pesp_conn = NULL;
  lcoap_userdata *cud;
  int stack = 1;

  cud = (lcoap_userdata *)luaL_checkudata(L, stack, "coap_client");
  luaL_argcheck(L, cud, stack, "Server/Client expected");
  if(cud==NULL){
    NODE_DBG("userdata is nil.\n");
    return 0;
  }

  stack++;
  pesp_conn = cud->pesp_conn;
  ip_addr_t ipaddr;
  uint8_t host[64];

  unsigned t;
  if ( lua_isnumber(L, stack) )
  {
    t = lua_tointeger(L, stack);
    stack++;
    if ( t != COAP_TYPE_CON && t != COAP_TYPE_NONCON )
      return luaL_error( L, "wrong arg type" );
  } else {
    t = COAP_TYPE_CON; // default to CON
  }

  size_t l;
  const char *url = luaL_checklstring( L, stack, &l );
  stack++;
  if (url == NULL)
    return luaL_error( L, "wrong arg type" );

  coap_uri_t *uri = coap_new_uri(url, l);   // should call free(uri) somewhere
  if (uri == NULL)
    return luaL_error( L, "uri wrong format." );
  if (uri->host.length + 1 /* for the null */ > sizeof(host)) {
    return luaL_error(L, "host too long");
  }

  pesp_conn->proto.udp->remote_port = uri->port;
  NODE_DBG("UDP port is set: %d.\n", uri->port);
  pesp_conn->proto.udp->local_port = espconn_port();

  if(uri->host.length){
    c_memcpy(host, uri->host.s, uri->host.length);
    host[uri->host.length] = '\0';

    ipaddr.addr = ipaddr_addr(host);
    NODE_DBG("Host len(%d):", uri->host.length);
    NODE_DBG(host);
    NODE_DBG("\n");

    c_memcpy(pesp_conn->proto.udp->remote_ip, &ipaddr.addr, 4);
    NODE_DBG("UDP ip is set: ");
    NODE_DBG(IPSTR, IP2STR(&ipaddr.addr));
    NODE_DBG("\n");
  }

  coap_pdu_t *pdu = coap_new_pdu();   // should call coap_delete_pdu() somewhere
  if(!pdu){
    if(uri)
      c_free(uri);
    return luaL_error (L, "alloc fail");
  }

  const char *payload = NULL;
  l = 0;
  if( lua_isstring(L, stack) ){
    payload = luaL_checklstring( L, stack, &l );
    if (payload == NULL)
      l = 0;
  }

  coap_make_request(&(pdu->scratch), pdu->pkt, t, m, uri, payload, l);

#ifdef COAP_DEBUG
  coap_dumpPacket(pdu->pkt);
#endif

  int rc;
  if (0 != (rc = coap_build(pdu->msg.p, &(pdu->msg.len), pdu->pkt))){
    NODE_DBG("coap_build failed rc=%d\n", rc);
  }
  else
  {
#ifdef COAP_DEBUG
    NODE_DBG("Sending: ");
    coap_dump(pdu->msg.p, pdu->msg.len, true);
    NODE_DBG("\n");
#endif
    espconn_regist_recvcb(pesp_conn, coap_response_handler);
    sint8_t con = espconn_create(pesp_conn);
    if( ESPCONN_OK != con){
      NODE_DBG("Connect to host. code:%d\n", con);
      // coap_delete_pdu(pdu);
    } 
    // else 
    {
      coap_tid_t tid = COAP_INVALID_TID;
      if (pdu->pkt->hdr.t == COAP_TYPE_CON){
        tid = coap_send_confirmed(pesp_conn, pdu);
      }
      else {
        tid = coap_send(pesp_conn, pdu);
      }
      if (pdu->pkt->hdr.t != COAP_TYPE_CON || tid == COAP_INVALID_TID){
        coap_delete_pdu(pdu);
      }
    }
  }

  if(uri)
    c_free((void *)uri);

  NODE_DBG("coap_request is called.\n");
  return 0;  
}

extern coap_luser_entry *variable_entry;
extern coap_luser_entry *function_entry;
// Lua: coap:var/func( string )
static int coap_regist( lua_State* L, const char* mt, int isvar )
{
  size_t l;
  const char *name = luaL_checklstring( L, 2, &l );
  int content_type = luaL_optint(L, 3, COAP_CONTENTTYPE_TEXT_PLAIN);
  if (name == NULL)
    return luaL_error( L, "name must be set." );

  coap_luser_entry *h = NULL;
  // if(lua_isstring(L, 3))
  if(isvar)
    h = variable_entry;
  else
    h = function_entry;

  while(NULL!=h->next){  // goto the end of the list
    if(h->name!= NULL && c_strcmp(h->name, name)==0)  // key exist, override it
      break;
    h = h->next;
  }

  if(h->name==NULL || c_strcmp(h->name, name)!=0){   // not exists. make a new one.
    h->next = (coap_luser_entry *)c_zalloc(sizeof(coap_luser_entry));
    h = h->next;
    if(h == NULL)
      return luaL_error(L, "not enough memory");
    h->next = NULL;
    h->name = NULL;
  }  

  h->name = name;
  h->content_type = content_type;

  NODE_DBG("coap_regist is called.\n");
  return 0;  
}

// Lua: s = coap.createServer(function(conn))
static int coap_createServer( lua_State* L )
{
  const char *mt = "coap_server";
  return coap_create(L, mt);
}

// Lua: server:delete()
static int coap_server_delete( lua_State* L )
{
  const char *mt = "coap_server";
  return coap_delete(L, mt);
}

// Lua: server:listen( port, ip, function(err) )
static int coap_server_listen( lua_State* L )
{
  const char *mt = "coap_server";
  return coap_start(L, mt);
}

// Lua: server:close()
static int coap_server_close( lua_State* L )
{
  const char *mt = "coap_server";
  return coap_close(L, mt);
}

// Lua: server:on( "method", function(server) )
static int coap_server_on( lua_State* L )
{
  const char *mt = "coap_server";
  return coap_on(L, mt);
}

// Lua: server:var( "name" )
static int coap_server_var( lua_State* L )
{
  const char *mt = "coap_server";
  return coap_regist(L, mt, 1);
}

// Lua: server:func( "name" )
static int coap_server_func( lua_State* L )
{
  const char *mt = "coap_server";
  return coap_regist(L, mt, 0);
}

// Lua: s = coap.createClient(function(conn))
static int coap_createClient( lua_State* L )
{
  const char *mt = "coap_client";
  return coap_create(L, mt);
}

// Lua: client:gcdelete()
static int coap_client_gcdelete( lua_State* L )
{
  const char *mt = "coap_client";
  return coap_delete(L, mt);
}

// client:get( string, function(sent) )
static int coap_client_get( lua_State* L )
{
  return coap_request(L, COAP_METHOD_GET);
}

// client:post( string, function(sent) )
static int coap_client_post( lua_State* L )
{
  return coap_request(L, COAP_METHOD_POST);
}

// client:put( string, function(sent) )
static int coap_client_put( lua_State* L )
{
  return coap_request(L, COAP_METHOD_PUT);
}

// client:delete( string, function(sent) )
static int coap_client_delete( lua_State* L )
{
  return coap_request(L, COAP_METHOD_DELETE);
}

// Module function map
static const LUA_REG_TYPE coap_server_map[] = {
  { LSTRKEY( "listen" ),  LFUNCVAL( coap_server_listen ) },
  { LSTRKEY( "close" ),   LFUNCVAL( coap_server_close ) },
  { LSTRKEY( "var" ),     LFUNCVAL( coap_server_var ) },
  { LSTRKEY( "func" ),    LFUNCVAL( coap_server_func ) },
  { LSTRKEY( "__gc" ),    LFUNCVAL( coap_server_delete ) },
  { LSTRKEY( "__index" ), LROVAL( coap_server_map ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE coap_client_map[] = {
  { LSTRKEY( "get" ),     LFUNCVAL( coap_client_get ) },
  { LSTRKEY( "post" ),    LFUNCVAL( coap_client_post ) },
  { LSTRKEY( "put" ),     LFUNCVAL( coap_client_put ) },
  { LSTRKEY( "delete" ),  LFUNCVAL( coap_client_delete ) },
  { LSTRKEY( "__gc" ),    LFUNCVAL( coap_client_gcdelete ) },
  { LSTRKEY( "__index" ), LROVAL( coap_client_map ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE coap_map[] = 
{
  { LSTRKEY( "Server" ),      LFUNCVAL( coap_createServer ) },
  { LSTRKEY( "Client" ),      LFUNCVAL( coap_createClient ) },
  { LSTRKEY( "CON" ),         LNUMVAL( COAP_TYPE_CON ) },
  { LSTRKEY( "NON" ),         LNUMVAL( COAP_TYPE_NONCON ) },
  { LSTRKEY( "TEXT_PLAIN"),   LNUMVAL( COAP_CONTENTTYPE_TEXT_PLAIN ) },
  { LSTRKEY( "LINKFORMAT"),   LNUMVAL( COAP_CONTENTTYPE_APPLICATION_LINKFORMAT ) },
  { LSTRKEY( "XML"),          LNUMVAL( COAP_CONTENTTYPE_APPLICATION_XML ) },
  { LSTRKEY( "OCTET_STREAM"), LNUMVAL( COAP_CONTENTTYPE_APPLICATION_OCTET_STREAM ) },
  { LSTRKEY( "EXI"),          LNUMVAL( COAP_CONTENTTYPE_APPLICATION_EXI ) },
  { LSTRKEY( "JSON"),         LNUMVAL( COAP_CONTENTTYPE_APPLICATION_JSON) },
  { LSTRKEY( "__metatable" ), LROVAL( coap_map ) },
  { LNILKEY, LNILVAL }
};

int luaopen_coap( lua_State *L )
{
  endpoint_setup();
  luaL_rometatable(L, "coap_server", (void *)coap_server_map);  // create metatable for coap_server 
  luaL_rometatable(L, "coap_client", (void *)coap_client_map);  // create metatable for coap_client  
  return 0;
}

NODEMCU_MODULE(COAP, "coap", coap_map, luaopen_coap);
