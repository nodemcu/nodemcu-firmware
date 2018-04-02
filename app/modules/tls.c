// Module for TLS

#include "module.h"

#if defined(CLIENT_SSL_ENABLE) && defined(LUA_USE_MODULES_NET)

#include "lauxlib.h"
#include "platform.h"
#include "lmem.h"

#include "c_string.h"
#include "c_stdlib.h"

#include "c_types.h"
#include "mem.h"
#include "lwip/ip_addr.h"
#include "espconn.h"
#include "sys/espconn_mbedtls.h"
#include "lwip/err.h"
#include "lwip/dns.h"

#include "mbedtls/debug.h"
#include "user_mbedtls.h"

#ifdef HAVE_SSL_SERVER_CRT
#include HAVE_SSL_SERVER_CRT
#else
__attribute__((section(".servercert.flash"))) unsigned char tls_server_cert_area[INTERNAL_FLASH_SECTOR_SIZE];
#endif

__attribute__((section(".clientcert.flash"))) unsigned char tls_client_cert_area[INTERNAL_FLASH_SECTOR_SIZE];

extern int tls_socket_create( lua_State *L );
extern const LUA_REG_TYPE tls_cert_map[];

typedef struct {
  struct espconn *pesp_conn;
  int self_ref;
  int cb_connect_ref;
  int cb_reconnect_ref;
  int cb_disconnect_ref;
  int cb_sent_ref;
  int cb_receive_ref;
  int cb_dns_ref;
} tls_socket_ud;

int tls_socket_create( lua_State *L ) {
  tls_socket_ud *ud = (tls_socket_ud*) lua_newuserdata(L, sizeof(tls_socket_ud));

  ud->pesp_conn = NULL;
  ud->self_ref =
  ud->cb_connect_ref =
  ud->cb_reconnect_ref =
  ud->cb_disconnect_ref =
  ud->cb_sent_ref =
  ud->cb_receive_ref =
  ud->cb_dns_ref = LUA_NOREF;

  luaL_getmetatable(L, "tls.socket");
  lua_setmetatable(L, -2);

  return 1;
}

static void tls_socket_onconnect( struct espconn *pesp_conn ) {
  tls_socket_ud *ud = (tls_socket_ud *)pesp_conn->reverse;
  if (!ud || ud->self_ref == LUA_NOREF) return;
  if (ud->cb_connect_ref != LUA_NOREF) {
    lua_State *L = lua_getstate();
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cb_connect_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    lua_call(L, 1, 0);
  }
}

static void tls_socket_cleanup(tls_socket_ud *ud) {
  if (ud->pesp_conn) {
    espconn_secure_disconnect(ud->pesp_conn);
    if (ud->pesp_conn->proto.tcp) {
      c_free(ud->pesp_conn->proto.tcp);
      ud->pesp_conn->proto.tcp = NULL;
    }
    c_free(ud->pesp_conn);
    ud->pesp_conn = NULL;
  }
  lua_State *L = lua_getstate();
  lua_gc(L, LUA_GCSTOP, 0);
  luaL_unref(L, LUA_REGISTRYINDEX, ud->self_ref);
  ud->self_ref = LUA_NOREF;
  lua_gc(L, LUA_GCRESTART, 0);
}

static void tls_socket_ondisconnect( struct espconn *pesp_conn ) {
  tls_socket_ud *ud = (tls_socket_ud *)pesp_conn->reverse;
  if (!ud || ud->self_ref == LUA_NOREF) return;
  tls_socket_cleanup(ud);
  if (ud->cb_disconnect_ref != LUA_NOREF) {
    lua_State *L = lua_getstate();
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cb_disconnect_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    tls_socket_cleanup(ud);
    lua_call(L, 1, 0);
  } else tls_socket_cleanup(ud);
}

static void tls_socket_onreconnect( struct espconn *pesp_conn, s8 err ) {
  tls_socket_ud *ud = (tls_socket_ud *)pesp_conn->reverse;
  if (!ud || ud->self_ref == LUA_NOREF) return;
  if (ud->cb_reconnect_ref != LUA_NOREF) {
    const char* reason = NULL;
    switch (err) {
      case(ESPCONN_MEM): reason = "Out of memory"; break;
      case(ESPCONN_TIMEOUT): reason = "Timeout"; break;
      case(ESPCONN_RTE): reason = "Routing problem"; break;
      case(ESPCONN_ABRT): reason = "Connection aborted"; break;
      case(ESPCONN_RST): reason = "Connection reset"; break;
      case(ESPCONN_CLSD): reason = "Connection closed"; break;
      case(ESPCONN_HANDSHAKE): reason = "SSL handshake failed"; break;
      case(ESPCONN_SSL_INVALID_DATA): reason = "SSL application invalid"; break;
    }
    lua_State *L = lua_getstate();
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cb_reconnect_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    if (reason != NULL) {
      lua_pushstring(L, reason);
    } else {
      lua_pushnil(L);
    }
    tls_socket_cleanup(ud);
    lua_call(L, 2, 0);
  } else tls_socket_cleanup(ud);
}

static void tls_socket_onrecv( struct espconn *pesp_conn, char *buf, u16 length ) {
  tls_socket_ud *ud = (tls_socket_ud *)pesp_conn->reverse;
  if (!ud || ud->self_ref == LUA_NOREF) return;
  if (ud->cb_receive_ref != LUA_NOREF) {
    lua_State *L = lua_getstate();
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cb_receive_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    lua_pushlstring(L, buf, length);
    lua_call(L, 2, 0);
  }
}

static void tls_socket_onsent( struct espconn *pesp_conn ) {
  tls_socket_ud *ud = (tls_socket_ud *)pesp_conn->reverse;
  if (!ud || ud->self_ref == LUA_NOREF) return;
  if (ud->cb_sent_ref != LUA_NOREF) {
    lua_State *L = lua_getstate();
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cb_sent_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    lua_call(L, 1, 0);
  }
}

static void tls_socket_dns_cb( const char* domain, const ip_addr_t *ip_addr, tls_socket_ud *ud ) {
  if (ud->self_ref == LUA_NOREF) return;
  ip_addr_t addr;
  if (ip_addr) addr = *ip_addr;
  else addr.addr = 0xFFFFFFFF;
  lua_State *L = lua_getstate();
  if (ud->cb_dns_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cb_dns_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
    if (addr.addr == 0xFFFFFFFF) {
      lua_pushnil(L);
    } else {
      char tmp[20];
      c_sprintf(tmp, IPSTR, IP2STR(&addr.addr));
      lua_pushstring(L, tmp);
    }
    lua_call(L, 2, 0);
  }
  if (addr.addr == 0xFFFFFFFF) {
    lua_gc(L, LUA_GCSTOP, 0);
    luaL_unref(L, LUA_REGISTRYINDEX, ud->self_ref);
    ud->self_ref = LUA_NOREF;
    lua_gc(L, LUA_GCRESTART, 0);
  } else {
    os_memcpy(ud->pesp_conn->proto.tcp->remote_ip, &addr.addr, 4);
    espconn_secure_connect(ud->pesp_conn);
  }
}

static int tls_socket_connect( lua_State *L ) {
  tls_socket_ud *ud = (tls_socket_ud *)luaL_checkudata(L, 1, "tls.socket");
  luaL_argcheck(L, ud, 1, "TLS socket expected");
  if(ud==NULL){
  	NODE_DBG("userdata is nil.\n");
  	return 0;
  }

  if (ud->pesp_conn) {
    return luaL_error(L, "already connected");
  }

  u16 port = luaL_checkinteger( L, 2 );
  size_t il;
  const char *domain = "127.0.0.1";
  if( lua_isstring(L, 3) )
    domain = luaL_checklstring( L, 3, &il );
  if (port == 0)
    return luaL_error(L, "invalid port");
  if (domain == NULL)
    return luaL_error(L, "invalid domain");

  ud->pesp_conn = (struct espconn*)c_zalloc(sizeof(struct espconn));
  if(!ud->pesp_conn)
    return luaL_error(L, "not enough memory");
  ud->pesp_conn->proto.udp = NULL;
  ud->pesp_conn->proto.tcp = (esp_tcp *)c_zalloc(sizeof(esp_tcp));
  if(!ud->pesp_conn->proto.tcp){
    c_free(ud->pesp_conn);
    ud->pesp_conn = NULL;
    return luaL_error(L, "not enough memory");
  }
  ud->pesp_conn->type = ESPCONN_TCP;
  ud->pesp_conn->state = ESPCONN_NONE;
  ud->pesp_conn->reverse = ud;
  ud->pesp_conn->proto.tcp->remote_port = port;
  espconn_regist_connectcb(ud->pesp_conn, (espconn_connect_callback)tls_socket_onconnect);
  espconn_regist_disconcb(ud->pesp_conn, (espconn_connect_callback)tls_socket_ondisconnect);
  espconn_regist_reconcb(ud->pesp_conn, (espconn_reconnect_callback)tls_socket_onreconnect);
  espconn_regist_recvcb(ud->pesp_conn, (espconn_recv_callback)tls_socket_onrecv);
  espconn_regist_sentcb(ud->pesp_conn, (espconn_sent_callback)tls_socket_onsent);

  if (ud->self_ref == LUA_NOREF) {
    lua_pushvalue(L, 1);  // copy to the top of stack
    ud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  ip_addr_t addr;
  err_t err = dns_gethostbyname(domain, &addr, (dns_found_callback)tls_socket_dns_cb, ud);
  if (err == ERR_OK) {
    tls_socket_dns_cb(domain, &addr, ud);
  } else if (err != ERR_INPROGRESS) {
    tls_socket_dns_cb(domain, NULL, ud);
  }

  return 0;
}

static int tls_socket_on( lua_State *L ) {
  tls_socket_ud *ud = (tls_socket_ud *)luaL_checkudata(L, 1, "tls.socket");
  luaL_argcheck(L, ud, 1, "TLS socket expected");
  if(ud==NULL){
  	NODE_DBG("userdata is nil.\n");
  	return 0;
  }

  size_t sl;
  const char *method = luaL_checklstring( L, 2, &sl );
  if (method == NULL)
    return luaL_error( L, "wrong arg type" );

  luaL_checkanyfunction(L, 3);
  lua_pushvalue(L, 3);  // copy argument (func) to the top of stack

  if (strcmp(method, "connection") == 0) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_connect_ref);
    ud->cb_connect_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else if (strcmp(method, "disconnection") == 0) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_disconnect_ref);
    ud->cb_disconnect_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else if (strcmp(method, "reconnection") == 0) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_reconnect_ref);
    ud->cb_reconnect_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else if (strcmp(method, "receive") == 0) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_receive_ref);
    ud->cb_receive_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else if (strcmp(method, "sent") == 0) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_sent_ref);
    ud->cb_sent_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else if (strcmp(method, "dns") == 0) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_dns_ref);
    ud->cb_dns_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else {
    return luaL_error(L, "invalid method");
  }
  return 0;
}

static int tls_socket_send( lua_State *L ) {
  tls_socket_ud *ud = (tls_socket_ud *)luaL_checkudata(L, 1, "tls.socket");
  luaL_argcheck(L, ud, 1, "TLS socket expected");
  if(ud==NULL){
  	NODE_DBG("userdata is nil.\n");
  	return 0;
  }

  if(ud->pesp_conn == NULL) {
    NODE_DBG("not connected");
    return 0;
  }

  size_t sl;
  const char* buf = luaL_checklstring(L, 2, &sl);
  if (!buf) {
    return luaL_error(L, "wrong arg type");
  }

  espconn_secure_send(ud->pesp_conn, (void*)buf, sl);

  return 0;
}
static int tls_socket_hold( lua_State *L ) {
  tls_socket_ud *ud = (tls_socket_ud *)luaL_checkudata(L, 1, "tls.socket");
  luaL_argcheck(L, ud, 1, "TLS socket expected");
  if(ud==NULL){
  	NODE_DBG("userdata is nil.\n");
  	return 0;
  }

  if(ud->pesp_conn == NULL) {
    NODE_DBG("not connected");
    return 0;
  }

  espconn_recv_hold(ud->pesp_conn);

  return 0;
}
static int tls_socket_unhold( lua_State *L ) {
  tls_socket_ud *ud = (tls_socket_ud *)luaL_checkudata(L, 1, "tls.socket");
  luaL_argcheck(L, ud, 1, "TLS socket expected");
  if(ud==NULL){
  	NODE_DBG("userdata is nil.\n");
  	return 0;
  }

  if(ud->pesp_conn == NULL) {
    NODE_DBG("not connected");
    return 0;
  }

  espconn_recv_unhold(ud->pesp_conn);

  return 0;
}
static int tls_socket_dns( lua_State *L ) {
  tls_socket_ud *ud = (tls_socket_ud *)luaL_checkudata(L, 1, "tls.socket");
  luaL_argcheck(L, ud, 1, "TLS socket expected");
  if(ud==NULL){
  	NODE_DBG("userdata is nil.\n");
  	return 0;
  }

  return 0;
}
static int tls_socket_getpeer( lua_State *L ) {
  tls_socket_ud *ud = (tls_socket_ud *)luaL_checkudata(L, 1, "tls.socket");
  luaL_argcheck(L, ud, 1, "TLS socket expected");
  if(ud==NULL){
  	NODE_DBG("userdata is nil.\n");
  	return 0;
  }

  if(ud->pesp_conn && ud->pesp_conn->proto.tcp->remote_port != 0){
    char temp[20] = {0};
    c_sprintf(temp, IPSTR, IP2STR( &(ud->pesp_conn->proto.tcp->remote_ip) ) );
    lua_pushstring( L, temp );
    lua_pushinteger( L, ud->pesp_conn->proto.tcp->remote_port );
  } else {
    lua_pushnil( L );
    lua_pushnil( L );
  }
  return 2;
}
static int tls_socket_close( lua_State *L ) {
  tls_socket_ud *ud = (tls_socket_ud *)luaL_checkudata(L, 1, "tls.socket");
  luaL_argcheck(L, ud, 1, "TLS socket expected");
  if(ud==NULL){
  	NODE_DBG("userdata is nil.\n");
  	return 0;
  }

  if (ud->pesp_conn) {
    espconn_secure_disconnect(ud->pesp_conn);
  }

  return 0;
}
static int tls_socket_delete( lua_State *L ) {
  tls_socket_ud *ud = (tls_socket_ud *)luaL_checkudata(L, 1, "tls.socket");
  luaL_argcheck(L, ud, 1, "TLS socket expected");
  if(ud==NULL){
  	NODE_DBG("userdata is nil.\n");
  	return 0;
  }
  if (ud->pesp_conn) {
    espconn_secure_disconnect(ud->pesp_conn);
    if (ud->pesp_conn->proto.tcp) {
      c_free(ud->pesp_conn->proto.tcp);
      ud->pesp_conn->proto.tcp = NULL;
    }
    c_free(ud->pesp_conn);
    ud->pesp_conn = NULL;
  }

  luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_connect_ref);
  ud->cb_connect_ref = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_disconnect_ref);
  ud->cb_disconnect_ref = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_reconnect_ref);
  ud->cb_reconnect_ref = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_dns_ref);
  ud->cb_dns_ref = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_receive_ref);
  ud->cb_receive_ref = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_sent_ref);
  ud->cb_sent_ref = LUA_NOREF;

  lua_gc(L, LUA_GCSTOP, 0);
  luaL_unref(L, LUA_REGISTRYINDEX, ud->self_ref);
  ud->self_ref = LUA_NOREF;
  lua_gc(L, LUA_GCRESTART, 0);

  return 0;
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

// Lua: tls.cert.auth(true / false | PEM data [, PEM data] )
static int tls_cert_auth(lua_State *L)
{
  int enable;

  uint32_t flash_offset = platform_flash_mapped2phys((uint32_t) &tls_client_cert_area[0]);
  if ((flash_offset & 0xfff) || flash_offset > 0xff000 || INTERNAL_FLASH_SECTOR_SIZE != 0x1000) {
    // THis should never happen
    return luaL_error( L, "bad offset" );
  }

  if (lua_type(L, 1) == LUA_TSTRING) {
    const char *types[3] = { "CERTIFICATE", "RSA PRIVATE KEY", NULL };
    const char *names[2] = { "certificate", "private_key" };
    const char *error = fill_page_with_pem(L, &tls_client_cert_area[0], flash_offset, types, names);
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
    if (tls_client_cert_area[0] == 0x00 || tls_client_cert_area[0] == 0xff) {
      return luaL_error( L, "no certificates found" );
    }
    rc = espconn_secure_cert_req_enable(1, flash_offset / INTERNAL_FLASH_SECTOR_SIZE);
  } else {
    rc = espconn_secure_cert_req_disable(1);
  }

  lua_pushboolean(L, rc);
  return 1;
}

// Lua: tls.cert.verify(true / false | PEM data [, PEM data] )
static int tls_cert_verify(lua_State *L)
{
  int enable;

  uint32_t flash_offset = platform_flash_mapped2phys((uint32_t) &tls_server_cert_area[0]);
  if ((flash_offset & 0xfff) || flash_offset > 0xff000 || INTERNAL_FLASH_SECTOR_SIZE != 0x1000) {
    // THis should never happen
    return luaL_error( L, "bad offset" );
  }

  if (lua_type(L, 1) == LUA_TSTRING) {
    const char *types[2] = { "CERTIFICATE", NULL };
    const char *names[1] = { "certificate" };

    const char *error = fill_page_with_pem(L, &tls_server_cert_area[0], flash_offset, types, names);
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
    if (tls_server_cert_area[0] == 0x00 || tls_server_cert_area[0] == 0xff) {
      return luaL_error( L, "no certificates found" );
    }
    rc = espconn_secure_ca_enable(1, flash_offset / INTERNAL_FLASH_SECTOR_SIZE);
  } else {
    rc = espconn_secure_ca_disable(1);
  }

  lua_pushboolean(L, rc);
  return 1;
}

#if defined(MBEDTLS_DEBUG_C)
static int tls_set_debug_threshold(lua_State *L) {
  mbedtls_debug_set_threshold(luaL_checkint( L, 1 ));
  return 0;
}
#endif

static const LUA_REG_TYPE tls_socket_map[] = {
  { LSTRKEY( "connect" ), LFUNCVAL( tls_socket_connect ) },
  { LSTRKEY( "close" ),   LFUNCVAL( tls_socket_close ) },
  { LSTRKEY( "on" ),      LFUNCVAL( tls_socket_on ) },
  { LSTRKEY( "send" ),    LFUNCVAL( tls_socket_send ) },
  { LSTRKEY( "hold" ),    LFUNCVAL( tls_socket_hold ) },
  { LSTRKEY( "unhold" ),  LFUNCVAL( tls_socket_unhold ) },
  { LSTRKEY( "dns" ),     LFUNCVAL( tls_socket_dns ) },
  { LSTRKEY( "getpeer" ), LFUNCVAL( tls_socket_getpeer ) },
  { LSTRKEY( "__gc" ),    LFUNCVAL( tls_socket_delete ) },
  { LSTRKEY( "__index" ), LROVAL( tls_socket_map ) },
  { LNILKEY, LNILVAL }
};

const LUA_REG_TYPE tls_cert_map[] = {
  { LSTRKEY( "verify" ),           LFUNCVAL( tls_cert_verify ) },
  { LSTRKEY( "auth" ),             LFUNCVAL( tls_cert_auth ) },
  { LSTRKEY( "__index" ),          LROVAL( tls_cert_map ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE tls_map[] = {
  { LSTRKEY( "createConnection" ), LFUNCVAL( tls_socket_create ) },
#if defined(MBEDTLS_DEBUG_C)
  { LSTRKEY( "setDebug" ),         LFUNCVAL( tls_set_debug_threshold ) },
#endif
  { LSTRKEY( "cert" ),             LROVAL( tls_cert_map ) },
  { LSTRKEY( "__metatable" ),      LROVAL( tls_map ) },
  { LNILKEY, LNILVAL }
};

int luaopen_tls( lua_State *L ) {
  luaL_rometatable(L, "tls.socket", (void *)tls_socket_map);  // create metatable for net.server
  espconn_secure_set_size(ESPCONN_CLIENT, 4096);
  return 0;
}

NODEMCU_MODULE(TLS, "tls", tls_map, luaopen_tls);
#endif
