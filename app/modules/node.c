// Module for interfacing with system
#include "module.h"
#include "lauxlib.h"
#include "lmem.h"

#include "platform.h"
#include <stdint.h>
#include <string.h>
#include "user_interface.h"
#include "flash_api.h"
#include "vfs.h"
#include "user_version.h"
#include "rom.h"
#include "task/task.h"

#define CPU80MHZ 80
#define CPU160MHZ 160

#define DELAY2SEC 2000

#ifndef LUA_MAXINTEGER
#define LUA_MAXINTEGER INT_MAX
#endif

static void restart_callback(void *arg) {
  UNUSED(arg);
  system_restart();
}
static int default_onerror(lua_State *L) {
  static os_timer_t restart_timer = {0};
  /* Use Lua print to print the ToS */
  lua_settop(L, 1);
  lua_getglobal(L, "print");
  lua_insert(L, 1);
  lua_pcall(L, 1, 0, 0);
  /* One first time through set automatic restart after 2s delay */
  if (!restart_timer.timer_func) {
    os_timer_setfn(&restart_timer, restart_callback, NULL);
		os_timer_arm(&restart_timer, DELAY2SEC, 0);
  }
  return 0;
}

LUAI_FUNC int lua_lfsreload_deprecated (lua_State *L) {
  platform_print_deprecation_note("node.flashreload", "soon. Use node.LFS interface instead");
  return node_lfsreload (L);
}

// Lua: setonerror([function])
static int node_setonerror( lua_State* L ) {
  lua_settop(L, 1);
  if (!lua_isfunction(L, 1)) {
    lua_pop(L, 1);
    lua_pushcfunction(L, default_onerror);
  }  
  lua_setfield(L, LUA_REGISTRYINDEX, "onerror");
  return 0;
}


// Lua: startupcommand(string)
static int node_startupcommand( lua_State* L ) {
  size_t l, lrcr;
  const char *cmd = luaL_checklstring(L, 1, &l);
  lrcr = platform_rcr_write(PLATFORM_RCR_INITSTR, cmd, l+1);
  lua_pushboolean(L, lrcr == ~0 ? 0 : 1);
  return 1;
}


// Lua: restart()
static int node_restart( lua_State* L )
{
  system_restart();
  return 0;
}

static int dsleepMax( lua_State *L ) {
  uint64_t dsm = (((uint64_t)system_rtc_clock_cali_proc())*(0x80000000-1))/0x1000;
  lua_pushnumber(L, (lua_Float) dsm);
  return 1;
}

// Lua: dsleep( us, option )
static int node_deepsleep( lua_State* L )
{
  uint64 us;
  uint8 option;
  // Set deleep option, skip if nil
  if ( lua_isnumber(L, 2) )
  {
    option = lua_tounsigned(L, 2);
    luaL_argcheck(L, 2, option <= 4, "wrong option value" );
    system_deep_sleep_set_option( option );
  }
  bool instant = (lua_isnumber(L, 3) && luaL_checkinteger(L, 3)) ? true: false;

  if ( lua_isnumber(L, 1) )
  {
#if LUA_VERSION_NUM == 501
    us = luaL_checknumber(L, 1);
#else /* 503 */
    us = lua_isinteger(L, 1) ? lua_tounsigned(L, 1) : (uint64) lua_tonumber(L, 1);
#endif
    luaL_argcheck(L, 1, us < 36000000000ull, "invalid time value" );
    if (instant)
      system_deep_sleep_instant(us);
    else
      system_deep_sleep(us);
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

static void add_int_field( lua_State* L, lua_Integer i, const char *name){
  lua_pushinteger(L, i);
  lua_setfield(L, -2, name);
}
static void add_string_field( lua_State* L, const char *s, const char *name) {
  lua_pushstring(L, s);
  lua_setfield(L, -2, name);
}

static void get_lfs_config ( lua_State* L ){
    int config[5];
    lua_getlfsconfig(L, config);
    lua_createtable(L, 0, 4);
    add_int_field(L, config[0], "lfs_mapped");
    add_int_field(L, config[1], "lfs_base");
    add_int_field(L, config[2], "lfs_size");
    add_int_field(L, config[3], "lfs_used");
}

static int node_info( lua_State* L ){
  const char* options[] = {"lfs", "hw", "sw_version", "build_config", "legacy", NULL};
  int option = luaL_checkoption (L, 1, options[4], options);

  switch (option) {
    case 0: { // lfs
      get_lfs_config(L);
      return 1;
    }
    case 1: { // hw
      lua_createtable(L, 0, 5);
      add_int_field(L, system_get_chip_id(),             "chip_id");
      add_int_field(L, spi_flash_get_id(),               "flash_id");
      add_int_field(L, flash_rom_get_size_byte() / 1024, "flash_size");
      add_int_field(L, flash_rom_get_mode(),             "flash_mode");
      add_int_field(L, flash_rom_get_speed(),            "flash_speed");
      return 1;
    }
    case 2: { // sw_version
      lua_createtable(L, 0, 7);     
      add_int_field(L, NODE_VERSION_MAJOR,       "node_version_major");
      add_int_field(L, NODE_VERSION_MINOR,       "node_version_minor");
      add_int_field(L, NODE_VERSION_REVISION,    "node_version_revision");
      add_string_field(L, BUILDINFO_BRANCH,      "git_branch");
      add_string_field(L, BUILDINFO_COMMIT_ID,   "git_commit_id");
      add_string_field(L, BUILDINFO_RELEASE,     "git_release");
      add_string_field(L, BUILDINFO_RELEASE_DTS, "git_commit_dts");
      return 1;
    }
    case 3: { // build_config
      lua_createtable(L, 0, 4);
      lua_pushboolean(L, BUILDINFO_SSL);
      lua_setfield(L, -2, "ssl");
      lua_pushinteger(L, BUILDINFO_LFS_SIZE);
      lua_setfield(L, -2, "lfs_size");
      add_string_field(L, BUILDINFO_MODULES, "modules");
      add_string_field(L, BUILDINFO_BUILD_TYPE, "number_type");
      return 1;
    }
    default: { // legacy
      platform_print_deprecation_note("node.info() without parameter", "in the next version");
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
  }
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

// Lua: input("string")
static int node_input( lua_State* L ) {
  luaL_checkstring(L, 1);
  lua_getfield(L, LUA_REGISTRYINDEX, "stdin");
  lua_rawgeti(L, -1, 1);             /* get the pipe_write func from stdin[1] */
  lua_insert(L, -2);                           /* and move above the pipe ref */
  lua_pushvalue(L, 1);
  lua_call(L, 2, 0);        /* stdin:write(line); errors are thrown to caller */
  return 0;
}

static int serial_debug = 1;

/*
** Output redirector. Note that panics in the output callback cannot be processed
** using luaL_pcallx() as this would create an infinite error loop, so they are
** reported direct to the UART.
*/
void output_redirect(const char *str, size_t l) {
  lua_State *L = lua_getstate();
  int n = lua_gettop(L);
  lua_pushliteral(L, "stdout");
  lua_rawget(L, LUA_REGISTRYINDEX);                       /* fetch reg.stdout */
  if (lua_istable(L, -1)) { /* reg.stdout is pipe */
    if (serial_debug) {
      uart0_sendStrn(str, l);
    }
    lua_rawgeti(L, -1, 1);          /* get the pipe_write func from stdout[1] */
    lua_insert(L, -2);                         /* and move above the pipe ref */
    lua_pushlstring(L, str, l);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {           /* Reg.stdout:write(str) */
      lua_writestringerror("error calling stdout:write(%s)\n", lua_tostring(L, -1));
      system_restart();
    }
  } else { /* reg.stdout == nil */
    uart0_sendStrn(str, l);
  }
  lua_settop(L, n);         /* Make sure all code paths leave stack unchanged */
}

extern int pipe_create(lua_State *L);

// Lua: output(function(c), debug)
static int node_output( lua_State* L )
{
  serial_debug = (lua_isnumber(L, 2) && lua_tointeger(L, 2) == 0) ? 0 : 1;
  lua_settop(L, 1);
  if (lua_isfunction(L, 1)) {
    lua_pushcfunction(L, pipe_create);
    lua_insert(L, 1);
    lua_pushinteger(L, LUA_TASK_MEDIUM);
    lua_call(L, 2, 1);      /* Any pipe.create() errors thrown back to caller */
  } else {    // remove the stdout pipe
    lua_pop(L,1);
    lua_pushnil(L);                                             /* T[1] = nil */
    serial_debug = 1;
  }
  lua_pushliteral(L, "stdout");
  lua_insert(L, 1);
  lua_rawset(L, LUA_REGISTRYINDEX);               /* Reg.stdout = nil or pipe */
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

#if LUA_VERSION_NUM == 501
#define getproto(o)	(clvalue(o)->l.p)
#endif

// Lua: compile(filename) -- compile lua file into lua bytecode, and save to .lc
static int node_compile( lua_State* L )
{
  Proto* f;
  int file_fd = 0;
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );
  const char *basename = vfs_basename( fname );
  luaL_argcheck(L, strlen(basename) <= FS_OBJ_NAME_LEN && strlen(fname) == len, 1, "filename invalid");

  char *output = luaM_malloc( L, len+1 );
  strcpy(output, fname);
  // check here that filename end with ".lua".
  if (len < 4 || (strcmp( output + len - 4, ".lua") != 0) ) {
    luaM_free( L, output );
    return luaL_error(L, "not a .lua file");
  }

  output[strlen(output) - 2] = 'c';
  output[strlen(output) - 1] = '\0';
  NODE_DBG(output);
  NODE_DBG("\n");
  if (luaL_loadfile(L, fname) != 0) {
    luaM_free( L, output );
    return luaL_error(L, lua_tostring(L, -1));
  }


  int stripping = 1;      /* strip debug information? */

  file_fd = vfs_open(output, "w+");
  if (!file_fd)
  {
    luaM_free( L, output );
    return luaL_error(L, "cannot open/write to file");
  }

  int result = lua_dump(L, writer, &file_fd, stripping);

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

// Lua: node.task.post([priority],task_cb) -- schedule a task for execution next
static int node_task_post( lua_State* L )
{
  int n=1;
  unsigned priority = TASK_PRIORITY_MEDIUM;
  if (lua_type(L, 1) == LUA_TNUMBER) {
    priority = (unsigned) luaL_checkint(L, 1);
    luaL_argcheck(L, priority <= TASK_PRIORITY_HIGH, 1, "invalid  priority");
    n++;
  }
  luaL_checktype(L, n, LUA_TFUNCTION);
  lua_settop(L, n);
  (void) luaL_posttask(L, priority);
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

  int n = lua_gettop(L);
  int strip = 0;

  lua_settop(L, 2);
  if (!lua_isnil(L, 1)) {
    strip = lua_tointeger(L, 1);
    luaL_argcheck(L, strip > 0 && strip < 4, 1, "Invalid strip level");
  } 

  if (lua_isnumber(L, 2)) {
    /* Use debug interface to replace stack level by corresponding function */
    int scope = luaL_checkinteger(L, 2);
    if (scope > 0) { 
      lua_Debug ar;
      lua_pop(L, 1);
      if (lua_getstack(L, scope, &ar)) {
        lua_getinfo(L, "f", &ar);  /* put function at [2] (ToS) */
      }
    }
  }
 
  int isfunc = lua_isfunction(L, 2);
  luaL_argcheck(L, n < 2 || isfunc, 2, "not a valid function");

  /* return result of lua_stripdebug, adding 1 if this is get/set level) */ 
  lua_pushinteger(L, lua_stripdebug(L, strip - 1) + (isfunc ? 0 : 1));
  return 1;
}


#if LUA_VERSION_NUM == 501
// Lua: node.egc.setmode( mode, [param])
// where the mode is one of the node.egc constants  NOT_ACTIVE , ON_ALLOC_FAILURE,
// ON_MEM_LIMIT, ALWAYS.  In the case of ON_MEM_LIMIT an integer parameter is reqired
// See legc.h and lecg.c.
static int node_egc_setmode(lua_State* L) {
  unsigned mode  = luaL_checkinteger(L, 1);
  int limit = luaL_optinteger (L, 2, 0);

  luaL_argcheck(L, mode <= (EGC_ON_ALLOC_FAILURE | EGC_ON_MEM_LIMIT | EGC_ALWAYS), 1, "invalid mode");
  luaL_argcheck(L, !(mode & EGC_ON_MEM_LIMIT) || limit!=0, 1, "limit must be non-zero");

  lua_setegcmode( L, mode, limit );
  return 0;
}
// totalallocated, estimatedused = node.egc.meminfo()
static int node_egc_meminfo(lua_State *L) {
  int totals[2];
  lua_getegcinfo(L, totals);
  lua_pushinteger(L, totals[0]);
  lua_pushinteger(L, totals[1]);
  return 2;
}
#endif
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

static lua_Unsigned random_value() {
  // Hopefully the compiler is smart enought to spot the constant IF check
  if (sizeof(lua_Unsigned) == 4) {
    return os_random();
  } else {
    return (((uint64_t) os_random()) << 32) + (uint32_t) os_random();
  }
}

lua_Integer node_random_range(lua_Integer l, lua_Integer u) {
  // The range is the number of different values to return
  lua_Unsigned range = u + 1 - l;

  // If this is very large then use simpler code
  if (range >= LUA_MAXINTEGER) {
    uint64_t v;

    // This cannot loop more than half the time
    while ((v = random_value()) >= range) {
    }

    // Now v is in the range [0, range)
    return v + l;
  }

  // Easy case, with only one value, we know the result
  if (range == 1) {
    return l;
  }

  // Another easy case -- uniform 32/64-bit
  if (range == 0) {
    return random_value();
  }

  // Now we have to figure out what a large multiple of range is
  // that just fits into 32/64 bits.
  // The limit will be less than 1 << 32 by some amount (not much)
  lua_Unsigned limit = (((1 + (lua_Unsigned) LUA_MAXINTEGER) / ((range + 1) >> 1)) - 1) * range;

  lua_Unsigned v;

  while ((v = random_value()) >= limit) {
  }

  // Now v is uniformly distributed in [0, limit) and limit is a multiple of range

  return (v % range) + l;
}

static int node_random (lua_State *L) {
  lua_Integer u;
  lua_Integer l;

  switch (lua_gettop(L)) {  /* check number of arguments */
    case 0: {  /* no arguments */
#ifdef LUA_NUMBER_INTEGRAL
      lua_pushinteger(L, 0);  /* Number between 0 and 1 - always 0 with ints */
#else
      lua_pushnumber(L, ((double)random_value() / 16 / (1LL << (8 * sizeof(lua_Unsigned) - 4))));
#endif
      return 1;
    }
    case 1: {  /* only upper limit */
      l = 1;
      u = luaL_checkinteger(L, 1);
      break;
    }
    case 2: {  /* lower and upper limits */
      l = luaL_checkinteger(L, 1);
      u = luaL_checkinteger(L, 2);
      break;
    }
    default:
      return luaL_error(L, "wrong number of arguments");
  }
  luaL_argcheck(L, l<=u, 2, "interval is empty");
  lua_pushinteger(L, node_random_range(l, u));  /* int between `l' and `u' */
  return 1;
}

#ifdef DEVELOPMENT_TOOLS
// Lua: rec = node.readrcr(id)
static int node_readrcr (lua_State *L) {
  int id  = luaL_checkinteger(L, 1);
  char *data;
  int n = platform_rcr_read(id, (void **)&data);
  if (n == ~0) return 0;
  lua_pushlstring(L, data, n);
  return 1;
}
// Lua: n = node.writercr(id,rec)
static int node_writercr (lua_State *L) {
  int id = luaL_checkinteger(L, 1),l;
  const char *data = lua_tolstring(L, 2, &l);
  int n = platform_rcr_write(id, data, l);
  lua_pushinteger(L, n);
  return 1;
}
#endif

// Lua: n = node.LFS.reload(lfsimage)
static int node_lfsreload (lua_State *L) {
  lua_settop(L, 1);
  luaL_lfsreload(L);
  return 1;
}

// Lua: n = node.flashindex(module)
// Lua: n = node.LFS.get(module)
static int node_lfsindex (lua_State *L) {
  lua_settop(L, 1);
  luaL_pushlfsmodule(L);
  return 1;
}

// Lua: n = node.LFS.list([option])
// Note that option is ignored in this release
static int node_lfslist (lua_State *L) {
  lua_settop(L, 1);
  luaL_pushlfsmodules(L);
  if (lua_istable(L, -1) && lua_getglobal(L, "table") == LUA_TTABLE) {
    lua_getfield(L, -1, "sort");
    lua_remove(L, -2);       /* remove table table */
    lua_pushvalue(L, -2);    /* dup array of modules ref to ToS */
    lua_call(L, 1, 0);
  }
  return 1;
}

//== node.LFS Table emulator ==============================================//

static int node_lfs_func (lua_State* L) {      /*T[1] = LFS, T[2] = fieldname */
  lua_remove(L, 1);
  lua_settop(L, 1);
  const char *name = lua_tostring(L, 1);
  if (!name) {
    lua_pushnil(L);
  } else if (!strcmp(name, "config")) {
    get_lfs_config(L);
  } else if (!strcmp(name, "time")) {
    luaL_pushlfsdts(L);
  } else {
    luaL_pushlfsmodule(L);
  }
  return 1;
}

LROT_BEGIN(node_lfs_meta, NULL, LROT_MASK_INDEX)
  LROT_FUNCENTRY( __index, node_lfs_func)
LROT_END(node_lfs_meta, NULL, LROT_MASK_INDEX)

LROT_BEGIN(node_lfs, LROT_TABLEREF(node_lfs_meta), 0)
  LROT_FUNCENTRY( list, node_lfslist)
  LROT_FUNCENTRY( get, node_lfsindex)
  LROT_FUNCENTRY( reload, node_lfsreload )
LROT_END(node_lfs, LROT_TABLEREF(node_lfs_meta), 0)


typedef enum pt_t { lfs_addr=0, lfs_size, spiffs_addr, spiffs_size, max_pt} pt_t;

LROT_BEGIN(pt_map, NULL, 0)
  LROT_NUMENTRY( lfs_addr, lfs_addr )
  LROT_NUMENTRY( lfs_size, lfs_size )
  LROT_NUMENTRY( spiffs_addr, spiffs_addr )
  LROT_NUMENTRY( spiffs_size, spiffs_size )
LROT_END(pt_map, NULL, 0)


// Lua: ptinfo = node.getpartitiontable()
static int node_getpartitiontable (lua_State *L) {
  uint32_t param[max_pt] = {0};
  param[lfs_size]    = platform_flash_get_partition(NODEMCU_LFS0_PARTITION, param + lfs_addr);
  param[spiffs_size] = platform_flash_get_partition(NODEMCU_SPIFFS0_PARTITION, param + spiffs_addr);

  lua_settop(L, 0);
  lua_createtable (L, 0, max_pt);                   /* at index 1 */
  lua_pushrotable(L, LROT_TABLEREF(pt_map));        /* at index 2 */
  lua_pushnil(L);                                   /* first key at index 3 */
  while (lua_next(L, 2) != 0) {                     /* key at index 3, and v at index 4 */
    lua_pushvalue(L, 3);                            /* dup key to index 5 */
    lua_pushinteger(L, param[lua_tointeger(L, 4)]); /* param [v] at index 6 */
    lua_rawset(L, 1);
    lua_pop(L, 1);                                  /* discard v */
  }
  lua_pop(L, 1);                                    /* discard pt_map reference */
  return 1;
}

static void insert_partition(partition_item_t *p, int n, uint32_t type, uint32_t addr) {
  if (n>0)
    memmove(p+1, p, n*sizeof(partition_item_t)); /* overlapped so must be move not cpy */
  p->type = type;
  p->addr = addr;
  p->size = 0;
}

static void delete_partition(partition_item_t *p, int n) {
  if (n>0)
    memmove(p, p+1, n*sizeof(partition_item_t)); /* overlapped so must be move not cpy */
}

#define SKIP (~0)
#define IROM0_PARTITION  (SYSTEM_PARTITION_CUSTOMER_BEGIN + NODEMCU_IROM0TEXT_PARTITION)
#define LFS_PARTITION    (SYSTEM_PARTITION_CUSTOMER_BEGIN + NODEMCU_LFS0_PARTITION)
#define SPIFFS_PARTITION (SYSTEM_PARTITION_CUSTOMER_BEGIN + NODEMCU_SPIFFS0_PARTITION)
#define SYSTEM_PARAMETER_SIZE  0x3000

// Lua: node.setpartitiontable(ptvals)
static int node_setpartitiontable (lua_State *L) {
  partition_item_t *rcr_pt = NULL, *pt;
  uint32_t flash_size = flash_rom_get_size_byte();
  uint32_t i = platform_rcr_read(PLATFORM_RCR_PT, (void **) &rcr_pt);
  uint32_t last = 0;
  uint32_t n = i / sizeof(partition_item_t);
  uint32_t param[max_pt] = {SKIP, SKIP, SKIP, SKIP};

/* stack 1=ptvals, 2=pt_map, 3=key, 4=ptval[key], 5=pt_map[key] */ 
  luaL_argcheck(L, lua_istable(L, 1), 1, "must be table");
  lua_settop(L, 1);
  /* convert input table into 4 option array */
  lua_pushrotable(L, LROT_TABLEREF(pt_map));       /* at index 2 */
  lua_pushnil(L);                                  /* first key at index 3 */
  while (lua_next(L, 1) != 0) {
    /* 'key' (at index 3) and 'value' (at index 4) */
    luaL_argcheck(L, lua_isstring(L, 3) && lua_isnumber(L, 4), 1, "invalid partition setting");
    lua_pushvalue(L, 3);  /* dup key to index 5 */
    lua_rawget(L, 2);     /* lookup in pt_map */
    luaL_argcheck(L, !lua_isnil(L, -1), 1, "invalid partition setting");
    param[lua_tointeger(L, 5)] = lua_tounsigned(L, 4);
    lua_pop(L, 2);  /* discard value and lookup; keeps 'key' for next iteration */
  }
 /*
  * Allocate a scratch Partition Table as userdata on the Lua stack, and copy the
  * current Flash PT into this for manipulation
  */
  lua_newuserdata(L, (n+2)*sizeof(partition_item_t));
  pt = lua_touserdata (L, -1);
  memcpy(pt, rcr_pt, n*sizeof(partition_item_t));
  pt[n].type = 0; pt[n+1].type = 0;

  for (i = 0; i < n; i ++) {
    partition_item_t *p = pt + i;

    if (p->type == IROM0_PARTITION && p[1].type != LFS_PARTITION) {
      // if the LFS partition is not following IROM0 then slot a blank one in
      insert_partition(p + 1, n-i-1, LFS_PARTITION, p->addr + p->size);
      n++;

    } else if (p->type == LFS_PARTITION) {
      // update the LFS options if set
      if (param[lfs_addr] != SKIP) 
        p->addr = param[lfs_addr];
      if (param[lfs_size] != SKIP) 
        p->size = param[lfs_size];
      if (p[1].type != SPIFFS_PARTITION) {
        // if the SPIFFS partition is not following LFS then slot a blank one in
        insert_partition(p + 1, n-i-1, SPIFFS_PARTITION, 0);
        n++;
      }

    } else if (p->type == SPIFFS_PARTITION) {
      // update the SPIFFS options if set
      if (param[spiffs_size] != SKIP) {
        p->size = param[spiffs_size];
        p->addr = (param[spiffs_addr] != SKIP) ? param[spiffs_addr] :
                    ((p->size <= flash_size - SYSTEM_PARAMETER_SIZE - 0x100000)
                     ? 0x100000 : last);
      } else if (param[spiffs_addr] != SKIP) {
        p->addr = param[spiffs_addr];
      }
#if 0
      if (param[spiffs_size] != SKIP) {
        // BOTCH: - at the moment the firmware doesn't boot if the SPIFFS partition
        //          is deleted so the minimum SPIFFS size is 64Kb
        p->size = param[spiffs_size] > 0x10000 ? param[spiffs_size] : 0x10000;
      }
#endif
    }

    if (p->size == 0) {
      // Delete 0-sized partitions as the SDK barfs on these
      delete_partition(p, n-i-1);
      n--; i--;
    } else {
      // Do consistency tests on the partition
      if (p->addr & (INTERNAL_FLASH_SECTOR_SIZE - 1) ||
        p->size & (INTERNAL_FLASH_SECTOR_SIZE - 1) ||
        p->addr < last ||
        p->addr + p->size > flash_size) {
        luaL_error(L, "Partition value out of range");
      }
      last = p->addr + p->size;
    }
  }
  for (i = 0; i < n; i ++)
    dbg_printf("Partition %d: %04x %06x %06x\n", i, pt[i].type, pt[i].addr, pt[i].size);

  platform_rcr_write(PLATFORM_RCR_PT, pt, n*sizeof(partition_item_t));
  while(1); // Trigger WDT; the new PT will be loaded on reboot

  return 0;
}


// Module function map
#if LUA_VERSION_NUM == 501
LROT_BEGIN(node_egc, NULL, 0)
  LROT_FUNCENTRY( meminfo, node_egc_meminfo )
  LROT_FUNCENTRY( setmode, node_egc_setmode )
  LROT_NUMENTRY( NOT_ACTIVE, EGC_NOT_ACTIVE )
  LROT_NUMENTRY( ON_ALLOC_FAILURE, EGC_ON_ALLOC_FAILURE )
  LROT_NUMENTRY( ON_MEM_LIMIT, EGC_ON_MEM_LIMIT )
  LROT_NUMENTRY( ALWAYS, EGC_ALWAYS )
LROT_END(node_egc, NULL, 0)
#endif

LROT_BEGIN(node_task, NULL, 0)
  LROT_FUNCENTRY( post, node_task_post )
  LROT_NUMENTRY( LOW_PRIORITY, TASK_PRIORITY_LOW )
  LROT_NUMENTRY( MEDIUM_PRIORITY, TASK_PRIORITY_MEDIUM )
  LROT_NUMENTRY( HIGH_PRIORITY, TASK_PRIORITY_HIGH )
LROT_END(node_task, NULL, 0)

LROT_BEGIN(node, NULL, 0)
  LROT_FUNCENTRY( heap, node_heap )
  LROT_FUNCENTRY( info, node_info )
  LROT_TABENTRY( task, node_task )
  LROT_FUNCENTRY( flashreload, lua_lfsreload_deprecated )
  LROT_FUNCENTRY( flashindex, node_lfsindex )
  LROT_TABENTRY( LFS, node_lfs )
  LROT_FUNCENTRY( setonerror, node_setonerror )
  LROT_FUNCENTRY( startupcommand, node_startupcommand )
  LROT_FUNCENTRY( restart, node_restart )
  LROT_FUNCENTRY( dsleep, node_deepsleep )
  LROT_FUNCENTRY( dsleepMax, dsleepMax )
  LROT_FUNCENTRY( sleep, node_sleep )
#ifdef PMSLEEP_ENABLE
  PMSLEEP_INT_MAP
#endif
#ifdef DEVELOPMENT_TOOLS
  LROT_FUNCENTRY( readrcr, node_readrcr )
  LROT_FUNCENTRY( writercr, node_writercr )
#endif
  LROT_FUNCENTRY( chipid, node_chipid )
  LROT_FUNCENTRY( flashid, node_flashid )
  LROT_FUNCENTRY( flashsize, node_flashsize )
  LROT_FUNCENTRY( input, node_input )
  LROT_FUNCENTRY( output, node_output )
// Moved to adc module, use adc.readvdd33()
//  LROT_FUNCENTRY( readvdd33, node_readvdd33 )
  LROT_FUNCENTRY( compile, node_compile )
  LROT_NUMENTRY( CPU80MHZ, CPU80MHZ )
  LROT_NUMENTRY( CPU160MHZ, CPU160MHZ )
  LROT_FUNCENTRY( setcpufreq, node_setcpufreq )
  LROT_FUNCENTRY( getcpufreq, node_getcpufreq )
  LROT_FUNCENTRY( bootreason, node_bootreason )
  LROT_FUNCENTRY( restore, node_restore )
  LROT_FUNCENTRY( random, node_random )
  LROT_FUNCENTRY( stripdebug, node_stripdebug )
#if LUA_VERSION_NUM == 501
  LROT_TABENTRY( egc, node_egc )
#endif
#ifdef DEVELOPMENT_TOOLS
  LROT_FUNCENTRY( osprint, node_osprint )
#endif
  LROT_FUNCENTRY( getpartitiontable, node_getpartitiontable )
  LROT_FUNCENTRY( setpartitiontable, node_setpartitiontable )

// Combined to dsleep(us, option)
//  LROT_FUNCENTRY( dsleepsetoption, node_deepsleep_setoption )
LROT_END(node, NULL, 0)

int luaopen_node( lua_State *L ) {
  lua_settop(L, 0);
  return node_setonerror(L);  /* set default onerror action */
}

NODEMCU_MODULE(NODE, "node", node, luaopen_node);
