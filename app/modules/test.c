/* 
 * http://blog.mclemon.io/esp8266-contributing-to-the-nodemcu-ecosystem
 *
 * test.c - simple lua for playing about with
 */


#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "c_stdlib.h"

// this stuff all required for ping?
#include "lwip/ip_addr.h"
#include "espconn.h"
#include "lwip/dns.h" 
// #include "lwip/app/ping.h"
#include "lwip/app/ping.h"
#include "lwip/raw.h"
#include "c_stdio.h"



static lua_State *gL = NULL;
static int ping_callback_ref;   // do we really assign a pointer to a string? Portability??
static int ping_host_count;
static ip_addr_t ping_host_ip;


// test.identity() - takes a single value, returns it
static int test_identity(lua_State *L) {  
  return 1;
}



// test.add(x,y) - take two values and add them
static int test_add(lua_State *L) {
  double x = lua_tonumber(L, 1);
  double y = lua_tonumber(L, 2);
  lua_pushnumber(L, x+y);
  return 1;
} 



// https://github.com/smcl/nodemcu-firmware/blob/cc04aaf92c1c076c30ef0b0eee43b3f924137440/app/modules/test.c

void ping_received(void *arg, void *data) {
    // struct ping_msg *pingmsg = (struct ping_msg*)arg;
    // struct ping_option *pingopt = pingmsg->ping_opt;
    struct ping_option *pingopt = (struct ping_option*)arg;
    struct ping_msg *pingmsg = pingopt->parent_msg ; 

    struct ping_resp *pingresp = (struct ping_resp*)data;

    // c_printf("[wjr] what do we have in ping_recieved? | arg= %X | data= %X | pingmsg= %X | pingopt= %X | pingresp = %X | \n", 
    //	      arg, data, pingmsg, pingopt, pingresp);
    
    char ipaddrstr[16];
    ip_addr_t source_ip;
    
    // c_printf("[wjr] before accessing pingopt\n");
    source_ip.addr = pingopt->ip;
    // c_printf("[wjr] after accessing pingopt\n");
    ipaddr_ntoa_r(&source_ip, ipaddrstr, sizeof(ipaddrstr));
    // c_printf("[wjr] after ipaddr_ntoa_r\n");

    // if we've registered a lua callback function, retrieve
    // it from registry + call it, otherwise just print the ping
    // response in a similar way to the standard iputils ping util
    if (ping_callback_ref != LUA_NOREF) {
      lua_rawgeti(gL, LUA_REGISTRYINDEX, ping_callback_ref);
      lua_pushinteger(gL, pingresp->bytes);
      lua_pushstring(gL, ipaddrstr);
      lua_pushinteger(gL, pingresp->seqno);
      lua_pushinteger(gL, pingresp->ttl);
      // lua_pushinteger(gL, 99999);
      lua_pushinteger(gL, pingresp->resp_time);
      lua_call(gL, 5, 0);
    } else {
      c_printf("%d bytes from %s, icmp_seq=%d ttl=%d time=%dms\n",
	       pingresp->bytes,
	       ipaddrstr,
	       pingresp->seqno,
	       pingresp->ttl,
	       // 99999,
	       pingresp->resp_time);
    }
}



static void ping_by_hostname(const char *name, ip_addr_t *ipaddr, void *arg) {
    struct ping_option *ping_opt = (struct ping_option *)c_zalloc(sizeof(struct ping_option));

    if (ipaddr->addr == IPADDR_NONE) {
      c_printf("problem resolving hostname\n");
	return;
    }
    
    ping_opt->count = ping_host_count;
    ping_opt->ip = ipaddr->addr;
    ping_opt->coarse_time = 0;
    ping_opt->recv_function = &ping_received;
    
    ping_start(ping_opt);
    // ping_start(ping_opt *);
}


/**
  * test.ping()
  * Description:
  * 	Send ICMP ping request to address, optionally call callback when response received
  * [wjr] is this stuff going into the doc automagically????
  * so this would be the proper way to write doc???
  *	         ...... foo bar tralalala .........
  *
  * Syntax:
  * 	wifi.sta.getconfig(ssid, password) --Set STATION configuration, Auto-connect by default, Connects to any BSSID
  *     test.ping(address)              -- send 4 ping requests to target address
  *     test.ping(address, n)           -- send n ping requests to target address
  *     test.ping(address, n, callback) -- send n ping requests to target address
  * Parameters:
  * 	address: string 
  * 	n: number of requests to send
  * 	callback: 
  * Returns:
  * 	Nothing.
  *
  * Example:
  *     test.ping("192.168.0.1")               -- send 4 pings to 192.168.0.1
  *     test.ping("192.168.0.1", 10)           -- send 10 pings to 192.168.0.1
  *     test.ping("192.168.0.1", 10, got_ping) -- send 10 pings to 192.168.0.1, call got_ping() with the
  *                                           --     ping results
  */

static int test_ping(lua_State *L)
{
    const char *ping_target;
    unsigned count = 4;

    // retrieve address arg (mandatory)
    if (lua_isstring(L, 1)) {
	ping_target = luaL_checkstring(L, 1);
    } else {
	return luaL_error(L, "no address specified");
    }

    // retrieve count arg (optional)
    if (lua_isnumber(L, 2)) {
	count = luaL_checkinteger(L, 2);
    }
   
    // c_printf("[wjr] checked number arg\n");
    
    // retrieve callback arg (optional)
    if (ping_callback_ref != LUA_NOREF) 
      luaL_unref(L, LUA_REGISTRYINDEX, ping_callback_ref);
    ping_callback_ref = LUA_NOREF;
       
    if (lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TLIGHTFUNCTION)
      ping_callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    
    gL = L;   // global L


    // c_printf("[wjr] checked callback arg\n");
    
    
    // attempt to parse ping target as IP
    uint32 ip = ipaddr_addr(ping_target);

    // c_printf("[wjr] checked IP conversion - result 0x%08X from %s \n", ip, ping_target);
    
    if (ip != IPADDR_NONE) {
	struct ping_option *ping_opt = (struct ping_option *)c_zalloc(sizeof(struct ping_option));

	ping_opt->count = count;
	ping_opt->ip = ip;
	ping_opt->coarse_time = 0;
	ping_opt->recv_function = &ping_received;

	// c_printf("[wjr] just befor calling ping_start | %i | 0x%08X  | %i | %X |\n", ping_opt->count, ping_opt->ip, ping_opt->coarse_time, ping_opt->recv_function);
	
	ping_start(ping_opt);
	
    } else {
        // return luaL_error(L, "[wjr] dns lookup ###ToDo");
	ping_host_count = count;

	struct espconn *ping_dns_lookup;
	espconn_create(ping_dns_lookup);
	espconn_gethostbyname(ping_dns_lookup, ping_target, &ping_host_ip, ping_by_hostname);	
    }

    return 0;
}



// Module function map
static const LUA_REG_TYPE test_map[] = {
  { LSTRKEY( "identity" ),         LFUNCVAL( test_identity ) },
  { LSTRKEY( "add" ),              LFUNCVAL( test_add ) },
  { LSTRKEY( "ping" ),             LFUNCVAL( test_ping ) },

  { LSTRKEY( "__metatable" ),      LROVAL( test_map ) },
  { LNILKEY, LNILVAL }
};


// Define an empty test function - we won't use this functionality
// should I know what this is for??? testing my module??
int luaopen_test( lua_State *L ) {
  return 0;
}



// Register the module - NODEMCU_MODULE() will make sure a module called "test" 
// is available when we define the LUA_USE_MODULES_TEST macro in user_modules.h
NODEMCU_MODULE(TEST, "test", test_map, luaopen_test);
// NODEMCU_MODULE(TEST, "test", test_map);


