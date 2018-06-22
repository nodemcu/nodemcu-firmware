// Module for interfacing with system

#include "module.h"
#include "lauxlib.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "legc.h"

#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"

#include "platform.h"
#include "lrodefs.h"
#ifdef LUA_FLASH_STORE
#include "lflash.h"
#endif
#include "c_types.h"
#include "c_string.h"
#include "driver/uart.h"
#include "user_interface.h"
#include "flash_api.h"
#include "vfs.h"
#include "user_version.h"
#include "rom.h"
#include "task/task.h"

#define CPU80MHZ 80
#define CPU160MHZ 160

// Lua: restart()
static int node_restart( lua_State* L )
{
  system_restart();
  return 0;
}

static int dsleepMax( lua_State *L ) {
  lua_pushnumber(L, (uint64_t)system_rtc_clock_cali_proc()*(0x80000000-1)/(0x1000));
  return 1;
}

// Lua: dsleep( us, option )
static int node_deepsleep( lua_State* L )
{
  uint64 us;
  uint8 option;
  //us = luaL_checkinteger( L, 1 );
  // Set deleep option, skip if nil
  if ( lua_isnumber(L, 2) )
  {
    option = lua_tointeger(L, 2);
    if ( option < 0 || option > 4)
      return luaL_error( L, "wrong arg range" );
    else
      system_deep_sleep_set_option( option );
  }
  bool instant = false;
  if (lua_isnumber(L, 3))
    instant = lua_tointeger(L, 3);
  // Set deleep time, skip if nil
  if ( lua_isnumber(L, 1) )
  {
    us = luaL_checknumber(L, 1);
    // if ( us <= 0 )
    if ( us < 0 )
      return luaL_error( L, "wrong arg range" );
    else
    {
      if (instant)
        system_deep_sleep_instant(us);
      else
        system_deep_sleep( us );
    }
  }
  return 0;
}


#ifdef PMSLEEP_ENABLE
#include "pm/pmSleep.h"

int node_sleep_resume_cb_ref= LUA_NOREF;
void node_sleep_resume_cb(void)
{
  PMSLEEP_DBG("START");
  pmSleep_execute_lua_cb(&node_sleep_resume_cb_ref);
  PMSLEEP_DBG("END");
}

// Lua: node.sleep(table)
static int node_sleep( lua_State* L )
{
#ifdef TIMER_SUSPEND_ENABLE
  pmSleep_INIT_CFG(cfg);
  cfg.sleep_mode=LIGHT_SLEEP_T;

  if(lua_istable(L, 1)){
    pmSleep_parse_table_lua(L, 1, &cfg, NULL, &node_sleep_resume_cb_ref);
  }
  else{
    return luaL_argerror(L, 1, "must be table");
  }

  cfg.resume_cb_ptr = &node_sleep_resume_cb;
  pmSleep_suspend(&cfg);
#else
  dbg_printf("\n The option \"TIMER_SUSPEND_ENABLE\" in \"app/include/user_config.h\" was disabled during FW build!\n");
  return luaL_error(L, "node.sleep() is unavailable");
#endif
  return 0;
}
#else
static int node_sleep( lua_State* L )
{
  dbg_printf("\n The options \"TIMER_SUSPEND_ENABLE\" and \"PMSLEEP_ENABLE\" in \"app/include/user_config.h\" were disabled during FW build!\n");
  return luaL_error(L, "node.sleep() is unavailable");
}
#endif //PMSLEEP_ENABLE
static int node_info( lua_State* L )
{
  lua_pushinteger(L, NODE_VERSION_MAJOR);
  lua_pushinteger(L, NODE_VERSION_MINOR);
  lua_pushinteger(L, NODE_VERSION_REVISION);
  lua_pushinteger(L, system_get_chip_id());   // chip id
  lua_pushinteger(L, spi_flash_get_id());     // flash id
  lua_pushinteger(L, flash_rom_get_size_byte() / 1024);  // flash size in KB
  lua_pushinteger(L, flash_rom_get_mode());
  lua_pushinteger(L, flash_rom_get_speed());
  return 8;
}

// Lua: chipid()
static int node_chipid( lua_State* L )
{
  uint32_t id = system_get_chip_id();
  lua_pushinteger(L, id);
  return 1;
}

// deprecated, moved to adc module
// Lua: readvdd33()
// static int node_readvdd33( lua_State* L )
// {
//   uint32_t vdd33 = readvdd33();
//   lua_pushinteger(L, vdd33);
//   return 1;
// }
// Lua: flashid()
static int node_flashid( lua_State* L )
{
  uint32_t id = spi_flash_get_id();
  lua_pushinteger( L, id );
  return 1;
}

// Lua: flashsize()
static int node_flashsize( lua_State* L )
{
  if (lua_type(L, 1) == LUA_TNUMBER)
  {
    flash_rom_set_size_byte(luaL_checkinteger(L, 1));
  }
  uint32_t sz = flash_rom_get_size_byte();
  lua_pushinteger( L, sz );
  return 1;
}

// Lua: heap()
static int node_heap( lua_State* L )
{
  uint32_t sz = system_get_free_heap_size();
  lua_pushinteger(L, sz);
  return 1;
}

extern int  lua_put_line(const char *s, size_t l);
extern bool user_process_input(bool force);

// Lua: input("string")
static int node_input( lua_State* L ) {
  size_t l = 0;
  const char *s = luaL_checklstring(L, 1, &l);
  if (lua_put_line(s, l)) {
    NODE_DBG("Result (if any):\n");
    user_process_input(true);
  }
  return 0;
}

static int output_redir_ref = LUA_NOREF;
static int serial_debug = 1;
void output_redirect(const char *str) {
  lua_State *L = lua_getstate();
  // if(c_strlen(str)>=TX_BUFF_SIZE){
  //   NODE_ERR("output too long.\n");
  //   return;
  // }

  if (output_redir_ref == LUA_NOREF) {
    uart0_sendStr(str);
    return;
  }

  if (serial_debug != 0) {
    uart0_sendStr(str);
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, output_redir_ref);
  lua_pushstring(L, str);
  lua_call(L, 1, 0);   // this call back function should never user output.
}

// Lua: output(function(c), debug)
static int node_output( lua_State* L )
{
  // luaL_checkanyfunction(L, 1);
  if (lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 1);  // copy argument (func) to the top of stack
    if (output_redir_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, output_redir_ref);
    output_redir_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else {    // unref the key press function
    if (output_redir_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, output_redir_ref);
    output_redir_ref = LUA_NOREF;
    serial_debug = 1;
    return 0;
  }

  if ( lua_isnumber(L, 2) )
  {
    serial_debug = lua_tointeger(L, 2);
    if (serial_debug != 0)
      serial_debug = 1;
  } else {
    serial_debug = 1; // default to 1
  }

  return 0;
}

static int writer(lua_State* L, const void* p, size_t size, void* u)
{
  UNUSED(L);
  int file_fd = *( (int *)u );
  if (!file_fd)
    return 1;
  NODE_DBG("get fd:%d,size:%d\n", file_fd, size);

  if (size != 0 && (size != vfs_write(file_fd, (const char *)p, size)) )
    return 1;
  NODE_DBG("write fd:%d,size:%d\n", file_fd, size);
  return 0;
}

#define toproto(L,i) (clvalue(L->top+(i))->l.p)
// Lua: compile(filename) -- compile lua file into lua bytecode, and save to .lc
static int node_compile( lua_State* L )
{
  Proto* f;
  int file_fd = 0;
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );
  const char *basename = vfs_basename( fname );
  luaL_argcheck(L, c_strlen(basename) <= FS_OBJ_NAME_LEN && c_strlen(fname) == len, 1, "filename invalid");

  char *output = luaM_malloc( L, len+1 );
  c_strcpy(output, fname);
  // check here that filename end with ".lua".
  if (len < 4 || (c_strcmp( output + len - 4, ".lua") != 0) ) {
    luaM_free( L, output );
    return luaL_error(L, "not a .lua file");
  }

  output[c_strlen(output) - 2] = 'c';
  output[c_strlen(output) - 1] = '\0';
  NODE_DBG(output);
  NODE_DBG("\n");
  if (luaL_loadfsfile(L, fname) != 0) {
    luaM_free( L, output );
    return luaL_error(L, lua_tostring(L, -1));
  }

  f = toproto(L, -1);

  int stripping = 1;      /* strip debug information? */

  file_fd = vfs_open(output, "w+");
  if (!file_fd)
  {
    luaM_free( L, output );
    return luaL_error(L, "cannot open/write to file");
  }

  lua_lock(L);
  int result = luaU_dump(L, f, writer, &file_fd, stripping);
  lua_unlock(L);

  if (vfs_flush(file_fd) != VFS_RES_OK) {
    // overwrite Lua error, like writer() does in case of a file io error
    result = 1;
  }
  vfs_close(file_fd);
  file_fd = 0;
  luaM_free( L, output );

  if (result == LUA_ERR_CC_INTOVERFLOW) {
    return luaL_error(L, "value too big or small for target integer type");
  }
  if (result == LUA_ERR_CC_NOTINTEGER) {
    return luaL_error(L, "target lua_Number is integral but fractional value found");
  }
  if (result == 1) {    // result status generated by writer() or fs_flush() fail
    return luaL_error(L, "writing to file failed");
  }

  return 0;
}

// Task callback handler for node.task.post()
static task_handle_t do_node_task_handle;
static void do_node_task (task_param_t task_fn_ref, uint8_t prio)
{
  lua_State* L = lua_getstate();
  lua_rawgeti(L, LUA_REGISTRYINDEX, (int)task_fn_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, (int)task_fn_ref);
  lua_pushinteger(L, prio);
  lua_call(L, 1, 0);
}

// Lua: node.task.post([priority],task_cb) -- schedule a task for execution next
static int node_task_post( lua_State* L )
{
  int n = 1, Ltype = lua_type(L, 1);
  unsigned priority = TASK_PRIORITY_MEDIUM;
  if (Ltype == LUA_TNUMBER) {
    priority = (unsigned) luaL_checkint(L, 1);
    luaL_argcheck(L, priority <= TASK_PRIORITY_HIGH, 1, "invalid  priority");
    Ltype = lua_type(L, ++n);
  }
  luaL_argcheck(L, Ltype == LUA_TFUNCTION || Ltype == LUA_TLIGHTFUNCTION, n, "invalid function");
  lua_pushvalue(L, n);

  int task_fn_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  if (!do_node_task_handle)  // bind the task handle to do_node_task on 1st call
    do_node_task_handle = task_get_id(do_node_task);

  if(!task_post(priority, do_node_task_handle, (task_param_t)task_fn_ref)) {
    luaL_unref(L, LUA_REGISTRYINDEX, task_fn_ref);
    luaL_error(L, "Task queue overflow. Task not posted");
  }
  return 0;
}

// Lua: setcpufreq(mhz)
// mhz is either CPU80MHZ od CPU160MHZ
static int node_setcpufreq(lua_State* L)
{
  // http://www.esp8266.com/viewtopic.php?f=21&t=1369
  uint32_t new_freq = luaL_checkinteger(L, 1);
  if (new_freq == CPU160MHZ){
    REG_SET_BIT(0x3ff00014, BIT(0));
    ets_update_cpu_frequency(CPU160MHZ);
  } else {
    REG_CLR_BIT(0x3ff00014,  BIT(0));
    ets_update_cpu_frequency(CPU80MHZ);
  }
  new_freq = ets_get_cpu_frequency();
  lua_pushinteger(L, new_freq);
  return 1;
}

// Lua: freq = node.getcpufreq()
static int node_getcpufreq(lua_State* L)
{
  lua_pushinteger(L, system_get_cpu_freq());
  return 1;
}

// Lua: code, reason [, exccause, epc1, epc2, epc3, excvaddr, depc ] = bootreason()
static int node_bootreason (lua_State *L)
{
  const struct rst_info *ri = system_get_rst_info ();
  uint32_t arr[8] = {
    rtc_get_reset_reason(),
    ri->reason,
    ri->exccause, ri->epc1, ri->epc2, ri->epc3, ri->excvaddr, ri->depc
  };
  int i, n = ((ri->reason != REASON_EXCEPTION_RST) ? 2 : 8);
  for (i = 0; i < n; ++i)
    lua_pushinteger (L, arr[i]);
  return n;
}

// Lua: restore()
static int node_restore (lua_State *L)
{
  system_restore();
  return 0;
}

#ifdef LUA_OPTIMIZE_DEBUG
/* node.stripdebug([level[, function]]). 
 * level:    1 don't discard debug
 *           2 discard Local and Upvalue debug info
 *           3 discard Local, Upvalue and lineno debug info.
 * function: Function to be stripped as per setfenv except 0 not permitted.
 * If no arguments then the current default setting is returned.
 * If function is omitted, this is the default setting for future compiles
 * The function returns an estimated integer count of the bytes stripped.
 */
static int node_stripdebug (lua_State *L) {
  int level;

  if (L->top == L->base) {
    lua_pushlightuserdata(L, &luaG_stripdebug );
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      lua_pushinteger(L, LUA_OPTIMIZE_DEBUG);
    }
    return 1;
  }

  level = luaL_checkint(L, 1);
  if ((level <= 0) || (level > 3)) luaL_argerror(L, 1, "must in range 1-3");

  if (L->top == L->base + 1) {
    /* Store the default level in the registry if no function parameter */
    lua_pushlightuserdata(L, &luaG_stripdebug);
    lua_pushinteger(L, level);
    lua_settable(L, LUA_REGISTRYINDEX);
    lua_settop(L,0);
    return 0;
  }

  if (level == 1) {
    lua_settop(L,0);
    lua_pushinteger(L, 0);
    return 1;
  }

  if (!lua_isfunction(L, 2)) {
    int scope = luaL_checkint(L, 2);
    if (scope > 0) {
      /* if the function parameter is a +ve integer then climb to find function */
      lua_Debug ar;
      lua_pop(L, 1); /* pop level as getinfo will replace it by the function */
      if (lua_getstack(L, scope, &ar)) {
        lua_getinfo(L, "f", &ar);
      }
    }
  }

  if(!lua_isfunction(L, 2) || lua_iscfunction(L, -1)) luaL_argerror(L, 2, "must be a Lua Function");
  // lua_lock(L);
  Proto *f = clvalue(L->base + 1)->l.p;
  // lua_unlock(L);
  lua_settop(L,0);
  lua_pushinteger(L, luaG_stripdebug(L, f, level, 1));
  return 1;
}
#endif

// Lua: node.egc.setmode( mode, [param])
// where the mode is one of the node.egc constants  NOT_ACTIVE , ON_ALLOC_FAILURE,
// ON_MEM_LIMIT, ALWAYS.  In the case of ON_MEM_LIMIT an integer parameter is reqired
// See legc.h and lecg.c.
static int node_egc_setmode(lua_State* L) {
  unsigned mode  = luaL_checkinteger(L, 1);
  int limit = luaL_optinteger (L, 2, 0);

  luaL_argcheck(L, mode <= (EGC_ON_ALLOC_FAILURE | EGC_ON_MEM_LIMIT | EGC_ALWAYS), 1, "invalid mode");
  luaL_argcheck(L, !(mode & EGC_ON_MEM_LIMIT) || limit!=0, 1, "limit must be non-zero");

  legc_set_mode( L, mode, limit );
  return 0;
}

// totalallocated, estimatedused = node.egc.meminfo()
static int node_egc_meminfo(lua_State *L) {
  global_State *g = G(L);
  lua_pushinteger(L, g->totalbytes);
  lua_pushinteger(L, g->estimate);
  return 2;
}

//
// Lua: osprint(true/false)
// Allows you to turn on the native Espressif SDK printing
static int node_osprint( lua_State* L )
{
  if (lua_toboolean(L, 1)) {
    system_set_os_print(1);
  } else {
    system_set_os_print(0);
  }

  return 0;  
}

int node_random_range(int l, int u) {
  // The range is the number of different values to return
  unsigned int range = u + 1 - l;

  // If this is very large then use simpler code
  if (range >= 0x7fffffff) {
    unsigned int v;

    // This cannot loop more than half the time
    while ((v = os_random()) >= range) {
    }

    // Now v is in the range [0, range)
    return v + l;
  }

  // Easy case, with only one value, we know the result
  if (range == 1) {
    return l;
  }

  // Another easy case -- uniform 32-bit
  if (range == 0) {
    return os_random();
  }

  // Now we have to figure out what a large multiple of range is
  // that just fits into 32 bits.
  // The limit will be less than 1 << 32 by some amount (not much)
  uint32_t limit = ((0x80000000 / ((range + 1) >> 1)) - 1) * range;

  uint32_t v;

  while ((v = os_random()) >= limit) {
  }

  // Now v is uniformly distributed in [0, limit) and limit is a multiple of range

  return (v % range) + l;
}

static int node_random (lua_State *L) {
  int u;
  int l;

  switch (lua_gettop(L)) {  /* check number of arguments */
    case 0: {  /* no arguments */
#ifdef LUA_NUMBER_INTEGRAL
      lua_pushnumber(L, 0);  /* Number between 0 and 1 - always 0 with ints */
#else
      lua_pushnumber(L, (lua_Number)os_random() / (lua_Number)(1LL << 32));
#endif
      return 1;
    }
    case 1: {  /* only upper limit */
      l = 1;
      u = luaL_checkint(L, 1);
      break;
    }
    case 2: {  /* lower and upper limits */
      l = luaL_checkint(L, 1);
      u = luaL_checkint(L, 2);
      break;
    }
    default: 
      return luaL_error(L, "wrong number of arguments");
  }
  luaL_argcheck(L, l<=u, 2, "interval is empty");
  lua_pushnumber(L, node_random_range(l, u));  /* int between `l' and `u' */
  return 1;
}


// Module function map

static const LUA_REG_TYPE node_egc_map[] = {
  { LSTRKEY( "meminfo" ),           LFUNCVAL( node_egc_meminfo ) },
  { LSTRKEY( "setmode" ),           LFUNCVAL( node_egc_setmode ) },
  { LSTRKEY( "NOT_ACTIVE" ),        LNUMVAL( EGC_NOT_ACTIVE ) },
  { LSTRKEY( "ON_ALLOC_FAILURE" ),  LNUMVAL( EGC_ON_ALLOC_FAILURE ) },
  { LSTRKEY( "ON_MEM_LIMIT" ),      LNUMVAL( EGC_ON_MEM_LIMIT ) },
  { LSTRKEY( "ALWAYS" ),            LNUMVAL( EGC_ALWAYS ) },
  { LNILKEY, LNILVAL }
};
static const LUA_REG_TYPE node_task_map[] = {
  { LSTRKEY( "post" ),            LFUNCVAL( node_task_post ) },
  { LSTRKEY( "LOW_PRIORITY" ),    LNUMVAL( TASK_PRIORITY_LOW ) },
  { LSTRKEY( "MEDIUM_PRIORITY" ), LNUMVAL( TASK_PRIORITY_MEDIUM ) },
  { LSTRKEY( "HIGH_PRIORITY" ),   LNUMVAL( TASK_PRIORITY_HIGH ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE node_map[] =
{
  { LSTRKEY( "heap" ), LFUNCVAL( node_heap ) },
  { LSTRKEY( "info" ), LFUNCVAL( node_info ) },
  { LSTRKEY( "task" ), LROVAL( node_task_map ) },
#ifdef LUA_FLASH_STORE
  { LSTRKEY( "flashreload" ), LFUNCVAL( luaN_reload_reboot ) },
  { LSTRKEY( "flashindex" ), LFUNCVAL( luaN_index ) },
#endif
  { LSTRKEY( "restart" ),   LFUNCVAL( node_restart ) },
  { LSTRKEY( "dsleep" ),    LFUNCVAL( node_deepsleep ) },
  { LSTRKEY( "dsleepMax" ), LFUNCVAL( dsleepMax ) },
  { LSTRKEY( "sleep" ), LFUNCVAL( node_sleep ) },
#ifdef PMSLEEP_ENABLE
  PMSLEEP_INT_MAP,
#endif
  { LSTRKEY( "chipid" ), LFUNCVAL( node_chipid ) },
  { LSTRKEY( "flashid" ), LFUNCVAL( node_flashid ) },
  { LSTRKEY( "flashsize" ), LFUNCVAL( node_flashsize) },
  { LSTRKEY( "input" ), LFUNCVAL( node_input ) },
  { LSTRKEY( "output" ), LFUNCVAL( node_output ) },
// Moved to adc module, use adc.readvdd33()
// { LSTRKEY( "readvdd33" ), LFUNCVAL( node_readvdd33) },
  { LSTRKEY( "compile" ), LFUNCVAL( node_compile) },
  { LSTRKEY( "CPU80MHZ" ), LNUMVAL( CPU80MHZ ) },
  { LSTRKEY( "CPU160MHZ" ), LNUMVAL( CPU160MHZ ) },
  { LSTRKEY( "setcpufreq" ), LFUNCVAL( node_setcpufreq) },
  { LSTRKEY( "getcpufreq" ), LFUNCVAL( node_getcpufreq) },
  { LSTRKEY( "bootreason" ), LFUNCVAL( node_bootreason) },
  { LSTRKEY( "restore" ), LFUNCVAL( node_restore) },
  { LSTRKEY( "random" ), LFUNCVAL( node_random) },
#ifdef LUA_OPTIMIZE_DEBUG
  { LSTRKEY( "stripdebug" ), LFUNCVAL( node_stripdebug ) },
#endif
  { LSTRKEY( "egc" ),  LROVAL( node_egc_map ) },
#ifdef DEVELOPMENT_TOOLS
  { LSTRKEY( "osprint" ), LFUNCVAL( node_osprint ) },
#endif

// Combined to dsleep(us, option)
// { LSTRKEY( "dsleepsetoption" ), LFUNCVAL( node_deepsleep_setoption) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(NODE, "node", node_map, NULL);
