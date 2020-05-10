// ***************************************************************************
// net_info module for ESP8266 with nodeMCU
//
// Written by Lukas Voborsky (@voborsky) with great help by TerryE
// ***************************************************************************

// #define NODE_DEBUG
 
#include "net_info.h"

#include "module.h"
#include "lauxlib.h"

#include "lwip/ip_addr.h"
#include "espconn.h"
#include "lwip/dns.h" 
#include "lwip/app/ping.h"

/* 
ping_opt needs to be the first element of the structure. It is a workaround to pass the 
callback reference and self_ref to the ping_received function. Pointer the ping_option 
structure is equal to the pointer to net_info_ping_t structure.
*/
typedef struct {
  struct ping_option ping_opt; 
  uint32_t ping_callback_ref; 
 } net_info_ping_t;
typedef net_info_ping_t* ping_t;  

/*
 *  ping_received_sent(pingresp)
*/
#define LuaCBfunc  lua_upvalueindex(1)
#define nipUD      lua_upvalueindex(2)

static int ping_received_sent(lua_State *L) {
    struct ping_resp *resp = (struct ping_resp *) lua_touserdata (L, 1);
    ping_t nip  = (ping_t) lua_touserdata (L, nipUD);
    lua_pushvalue(L, LuaCBfunc);

    NODE_DBG("[net_info ping_received_sent] nip = %p\n", nip);
    
    if (resp == NULL) {  /* resolution failed so call the CB with 0 byte count to flag this */
        luaL_unref(L, LUA_REGISTRYINDEX, nip->ping_callback_ref);
        lua_pushinteger(L, 0);
        luaL_pcallx(L, 1, 0);
        return 0;
    }
    char ipaddrstr[16];
    ipaddr_ntoa_r((ip_addr_t *) &nip->ping_opt.ip, ipaddrstr, sizeof(ipaddrstr));

    if (resp->total_count == 0) { /* processing receive response */
        NODE_DBG("[ping_received] %s: resp_time=%d seqno=%d bytes=%d ping_err=%d\n",
                 ipaddrstr, resp->resp_time, resp->seqno, resp->bytes, resp->ping_err);
        lua_pushinteger(L, resp->bytes);
        lua_pushstring(L, ipaddrstr);
        lua_pushinteger(L, resp->seqno);
        lua_pushinteger(L, resp->ping_err == 0 ? resp->resp_time: -1);        
        luaL_pcallx(L, 4, 0);
    } else {  /* sent completion is currently a No-op */
        NODE_DBG("[ping_sent] %s: total_count=%d timeout_count=%d "
                 "total_bytes=%d total_time=%d\n",
                 ipaddrstr, resp->total_count, resp->timeout_count, 
                 resp->total_bytes, resp->total_time);
        luaL_unref(L, LUA_REGISTRYINDEX, nip->ping_callback_ref);
    }
    return 0;
}


/*
 *  Wrapper to call ping_received_sent(pingresp)
 */
static void ping_CB(net_info_ping_t *nip, struct ping_resp *pingresp) {
    NODE_DBG("[net_info ping_CB] nip = %p, nip->ping_callback_ref = %p, pingresp= %p\n", nip, nip->ping_callback_ref, pingresp);
    lua_State *L = lua_getstate();
    lua_rawgeti(L, LUA_REGISTRYINDEX, nip->ping_callback_ref);
    lua_pushlightuserdata(L, pingresp);   
    lua_call(L, 1, 0); // call the closure (ping_received_sent)
}

/*
 *  Wrapper to call ping_start using fully resolve IP4 address
 */
static void net_info_ping_raw(const char *name, ip_addr_t *ipaddr, ping_t nip) {
    NODE_DBG("[net_info_ping_raw] name = %s, ipaddr= %x\n", name, ipaddr);
    if (ipaddr) {
      char ipaddrstr[16];
      ipaddr_ntoa_r(ipaddr, ipaddrstr, sizeof(ipaddrstr));
      NODE_DBG("[net_info_ping_raw] ip: %s\n", ipaddrstr);
    }    
    lua_State *L = lua_getstate();

    if (!ipaddr || ipaddr->addr == 0xFFFFFFFF) {
        ping_CB(nip, NULL);  /* A NULL pinresp flags DNS resolution failure */
        return;
    }

    nip->ping_opt.ip = ipaddr->addr;     
    NODE_DBG("[net_info_ping_raw] calling ping_start\n");
    if (!ping_start(&(nip->ping_opt))) {
        luaL_unref(L, LUA_REGISTRYINDEX, nip->ping_callback_ref);
        luaL_error(L, "memory allocation error: cannot start ping");
    }
}

// Lua:  net.ping(domain, [count], callback)
int net_info_ping(lua_State *L)
{
    ip_addr_t addr;

    // retrieve function parameters
    const char *ping_target = luaL_checkstring(L, 1);
    bool isf2 = lua_isfunction(L, 2);
    lua_Integer l_count  = isf2 ? 0: luaL_optinteger(L, 2, 0);  /* use ping_start() default */
    lua_settop(L, isf2 ? 2 : 3);
    luaL_argcheck(L, lua_isfunction(L, -1), -1, "no callback specified");
    
    ping_t nip = (ping_t) memset(lua_newuserdata(L, sizeof(*nip)), 0, sizeof(*nip));

    /* Register C closure with 2 Upvals: (1) Lua CB function; (2) nip Userdata */ 
    lua_pushcclosure(L, ping_received_sent, 2); // stack has nip UD, callback; [-n, +1, m]

    nip->ping_callback_ref = luaL_ref(L, LUA_REGISTRYINDEX); // registers the closure to registry [-1, +0, m]
    nip->ping_opt.count = l_count;
    nip->ping_opt.coarse_time = 0;
    nip->ping_opt.recv_function = (ping_recv_function) &ping_CB;
    nip->ping_opt.sent_function = (ping_sent_function) &ping_CB;
    
    NODE_DBG("[net_info_ping] nip = %p, nip->ping_callback_ref = %p\n", nip, nip->ping_callback_ref);

    err_t err = dns_gethostbyname(ping_target, &addr, (dns_found_callback) net_info_ping_raw, nip);
    if (err != ERR_OK && err != ERR_INPROGRESS) {
        luaL_unref(L, LUA_REGISTRYINDEX, nip->ping_callback_ref);
        return luaL_error(L, "lwip error %d", err);
    }
    if (err == ERR_OK) {
        NODE_DBG("[net_info_ping] No DNS resolution needed\n");
        net_info_ping_raw(ping_target, &addr, nip);
    }
    return 0;
}
