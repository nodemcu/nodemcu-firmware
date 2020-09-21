#include "module.h"
#include "lauxlib.h"
#include "common.h"
#include "legc.h"
#include "lundump.h"
#include "platform.h"
#include "task/task.h"
#include "vfs.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "soc/efuse_reg.h"
#include "ldebug.h"
#include "esp_vfs.h"
#include "lnodeaux.h"
#include "lflash.h"
#include "user_version.h"

// Lua: node.chipid()
static int node_chipid( lua_State *L )
{
  // This matches the way esptool.py generates a chipid for the ESP32 as of
  // esptool commit e9e9179f6fc3f2ecfc568987d3224b5e53a05f06
  // Oddly, this drops the lowest byte what's effectively the MAC address, so
  // it would seem plausible to encounter up to 256 chips with the same chipid
  uint64_t word16 = REG_READ(EFUSE_BLK0_RDATA1_REG);
  uint64_t word17 = REG_READ(EFUSE_BLK0_RDATA2_REG);
  const uint64_t MAX_UINT24 = 0xffffff;
  uint64_t cid = ((word17 & MAX_UINT24) << 24) | ((word16 >> 8) & MAX_UINT24);
  char chipid[17] = { 0 };
  sprintf(chipid, "0x%llx", cid);
  lua_pushstring(L, chipid);
  return 1;
}


// Lua: node.heap()
static int node_heap( lua_State* L )
{
  uint32_t sz = esp_get_free_heap_size();
  lua_pushinteger(L, sz);
  return 1;
}

static int node_restart (lua_State *L)
{
   esp_restart ();
   return 0;
}


// Lua: node.dsleep (microseconds|{opts})
static int node_dsleep (lua_State *L)
{
  lua_settop(L, 1);
  bool enable_timer_wakeup = false;
  int64_t usecs = 0;
  int type = lua_type(L, 1);
  if (type == LUA_TNUMBER) {
    enable_timer_wakeup = true;
    usecs = lua_tointeger(L, 1);
  } else if (type == LUA_TTABLE) {
    // time options: us, secs
    lua_getfield(L, 1, "us");
    lua_getfield(L, 1, "secs");
    enable_timer_wakeup = !lua_isnil(L, 2) || !lua_isnil(L, 3);
    lua_pop(L, 2);
    if (enable_timer_wakeup) {
      usecs = opt_checkint(L, "us", 0);
      usecs += (int64_t)opt_checkint(L, "secs", 0) * 1000000;
    }

    // GPIO wakeup options: gpio = num|{num, num, ...}
    uint64_t pin_mask = 0;
    lua_getfield(L, -1, "gpio");
    type = lua_type(L, -1);
    if (type == LUA_TNUMBER) {
      pin_mask |= 1ULL << lua_tointeger(L, -1);
    } else if (type == LUA_TTABLE) {
      for (int i = 1; ; i++) {
        lua_rawgeti(L, -1, i);
        int pin = lua_tointeger(L, -1);
        lua_pop(L, 1);
        if (!pin) {
          break;
        }
        pin_mask |= 1ULL << pin;
      }
    }
    lua_pop(L, 1); // gpio

    // Check pin validity here to get better error messages
    for (int pin = 0; pin < GPIO_PIN_COUNT; pin++) {
      if (pin_mask & (1ULL << pin)) {
        if (!rtc_gpio_is_valid_gpio(pin)) {
          return luaL_error(L, "Pin %d is not an RTC GPIO and cannot be used for wakeup", pin);
        }
      }
    }

    int level = opt_checkint_range(L, "level", 1, 0, 1);
    bool pull = opt_checkbool(L, "pull", false);
    bool touch = opt_checkbool(L, "touch", false);

    if (opt_get(L, "isolate", LUA_TTABLE)) {
      for (int i = 1; ; i++) {
        lua_rawgeti(L, -1, i);
        if (lua_isnil(L, -1)) {
          lua_pop(L, 1);
          break;
        }
        int pin = lua_tointeger(L, -1);
        lua_pop(L, 1);
        int err = rtc_gpio_isolate(pin);
        if (err) {
          return luaL_error(L, "Error %d returned from rtc_gpio_isolate(%d)", err, pin);
        }
      }
      lua_pop(L, 1); // isolate table
    }

    if (pull) {
        // Keeping the peripheral domain powered keeps the pullups/downs working
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    }

    if (pin_mask) {
      esp_sleep_ext1_wakeup_mode_t mode = (level == 1) ?
        ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW;
      int err = esp_sleep_enable_ext1_wakeup(pin_mask, mode);
      if (err) {
        return luaL_error(L, "Error %d returned from esp_sleep_enable_ext1_wakeup", err);
      }
    }

    if (touch) {
      esp_sleep_enable_touchpad_wakeup();
    }

  } else {
    luaL_argerror(L, 1, "Expected integer or table");
  }

  if (enable_timer_wakeup) {
    esp_sleep_enable_timer_wakeup(usecs);
  }
  esp_deep_sleep_start();
  // Note, above call does not actually return
  return 0;
}

static void add_int_field( lua_State* L, lua_Integer i, const char *name){
  lua_pushinteger(L, i);
  lua_setfield(L, -2, name);
}

static void add_string_field( lua_State* L, const char *s, const char *name) {
  lua_pushstring(L, s);
  lua_setfield(L, -2, name);
}

static int node_info( lua_State* L ){
  const char* options[] = {"sw_version", "default", NULL};
  int option = luaL_checkoption (L, 1, options[1], options);

  switch (option) {
    case 0: // sw_version
    default: { // default
      lua_createtable(L, 0, 7);
      add_string_field(L, NODE_VERSION,          "node_version");
      add_int_field(L, NODE_VERSION_MAJOR,       "node_version_major");
      add_int_field(L, NODE_VERSION_MINOR,       "node_version_minor");
      add_int_field(L, NODE_VERSION_REVISION,    "node_version_revision");
      add_string_field(L, BUILDINFO_BRANCH,      "git_branch");
      add_string_field(L, BUILDINFO_COMMIT_ID,   "git_commit_id");
      add_string_field(L, BUILDINFO_RELEASE,     "git_release");
      add_string_field(L, BUILDINFO_RELEASE_DTS, "git_commit_dts");
      return 1;
    }
  }
}

extern lua_Load gLoad;
extern bool user_process_input(bool force);
// Lua: input("string")
static int node_input( lua_State* L )
{
  size_t l = 0;
  const char *s = luaL_checklstring(L, 1, &l);
  if (s != NULL && l > 0 && l < LUA_MAXINPUT - 1)
  {
    lua_Load *load = &gLoad;
    if (load->line_position == 0) {
      memcpy(load->line, s, l);
      load->line[l + 1] = '\0';
      load->line_position = strlen(load->line) + 1;
      load->done = 1;
      user_process_input(true);
    }
  }
  return 0;
}

// The implementation of node.output implies replacing stdout with a virtual write-only file of
// which we can capture fwrite calls.
// When there is any write to the replaced stdout, our function redir_write will be called.
// we can then invoke the lua callback.

static FILE *oldstdout;              // keep the old stdout, e.g., the uart0
lua_ref_t output_redir = LUA_NOREF;  // this will hold the Lua callback
int serial_debug = 0;                // whether or not to write also to uart
const char *VFS_REDIR = "/redir";    // virtual filesystem mount point

// redir_write will be called everytime any code writes to stdout when
// redirection is active
ssize_t redir_write(int fd, const void *data, size_t size) {
    if (serial_debug)  // if serial_debug is nonzero, write to uart
        fwrite(data, sizeof(char), size, oldstdout);

    if (output_redir != LUA_NOREF) {  // prepare lua call
        lua_State *L = lua_getstate();
        lua_rawgeti(L, LUA_REGISTRYINDEX, output_redir);  // push function reference
        lua_pushlstring(L, (char *)data, size);           // push data
        lua_pcall(L, 1, 0, 0);                            // invoke callback
    }
    return size;
}

// redir_open is called when fopen() is called on /redir/xxx
int redir_open(const char *path, int flags, int mode) {
    return 79;  // since we only have one "file", just return some fd number to make the VFS system happy
}

// Lua: node.output(func, serial_debug)
static int node_output(lua_State *L) {
    if (lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TLIGHTFUNCTION) {
        if (output_redir == LUA_NOREF) {
            // create an instance of a virtual filesystem so we can use fopen
            esp_vfs_t redir_fs = {
                .flags = ESP_VFS_FLAG_DEFAULT,
                .write = &redir_write,
                .open = &redir_open,
                .fstat = NULL,
                .close = NULL,
                .read = NULL,
            };
            // register this filesystem under the `/redir` namespace
            ESP_ERROR_CHECK(esp_vfs_register(VFS_REDIR, &redir_fs, NULL));
            oldstdout = stdout;              // save the previous stdout
            stdout = fopen(VFS_REDIR, "w");  // open the new one for writing
        } else {
            luaX_unset_ref(L, &output_redir);  // dereference previous callback
        }
        luaX_set_ref(L, 1, &output_redir);  // set the callback
    } else {
        if (output_redir != LUA_NOREF) {
            fclose(stdout);                                  // close the redirected stdout
            stdout = oldstdout;                              // restore original stdout
            ESP_ERROR_CHECK(esp_vfs_unregister(VFS_REDIR));  // unregister redir filesystem
            luaX_unset_ref(L, &output_redir);                // forget callback
        }
        serial_debug = 1;
        return 0;
    }

    // second parameter indicates whether output will also be sent to old stdout
    if (lua_isnumber(L, 2)) {
        serial_debug = lua_tointeger(L, 2);
        if (serial_debug != 0)
            serial_debug = 1;
    } else {
        serial_debug = 1;  // default to 1
    }

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
  int level;

  if (L->top == L->base) {
    lua_pushlightuserdata(L, &luaG_stripdebug );
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      lua_pushinteger(L, CONFIG_LUA_OPTIMIZE_DEBUG);
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


// Lua: node.egc.setmode( mode, [param])
// where the mode is one of the node.egc constants  NOT_ACTIVE , ON_ALLOC_FAILURE,
// ON_MEM_LIMIT, ALWAYS.  In the case of ON_MEM_LIMIT an integer parameter is reqired
// See legc.h and lecg.c.
static int node_egc_setmode(lua_State* L) {
  unsigned mode  = luaL_checkinteger(L, 1);
  unsigned limit = luaL_optinteger (L, 2, 0);

  luaL_argcheck(L, mode <= (EGC_ON_ALLOC_FAILURE | EGC_ON_MEM_LIMIT | EGC_ALWAYS), 1, "invalid mode");
  luaL_argcheck(L, !(mode & EGC_ON_MEM_LIMIT) || limit>0, 1, "limit must be non-zero");

  legc_set_mode( L, mode, limit );
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
  luaL_argcheck(L, strlen(basename) <= CONFIG_FS_OBJ_NAME_LEN && strlen(fname) == len, 1, "filename invalid");

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
static void do_node_task (task_param_t task_fn_ref, task_prio_t prio)
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


static int node_osprint (lua_State *L)
{
  if (lua_toboolean (L, 1))
    esp_log_level_set ("*", CONFIG_LOG_DEFAULT_LEVEL);
  else
    esp_log_level_set ("*", ESP_LOG_NONE);
  return 0;
}


static int node_uptime(lua_State *L)
{
  uint64_t now = esp_timer_get_time();
#ifdef LUA_NUMBER_INTEGRAL
  lua_pushinteger(L, (lua_Integer)(now & 0x7FFFFFFF));
  lua_pushinteger(L, (lua_Integer)((now >> 31) & 0x7FFFFFFF));
#else
  // The largest double that doesn't lose whole-number precision is 2^53, so the
  // mask we apply is (2^53)-1 which is 0x1FFFFFFFFFFFFF. In practice this is
  // long enough the timer should never wrap, but it interesting nonetheless.
  lua_pushnumber(L, (lua_Number)(now & 0x1FFFFFFFFFFFFFull));
  lua_pushinteger(L, (lua_Integer)(now >> 53));
#endif
  return 2;
}


LROT_BEGIN(node_egc)
  LROT_FUNCENTRY( setmode,           node_egc_setmode )
  LROT_NUMENTRY ( NOT_ACTIVE,        EGC_NOT_ACTIVE )
  LROT_NUMENTRY ( ON_ALLOC_FAILURE,  EGC_ON_ALLOC_FAILURE )
  LROT_NUMENTRY ( ON_MEM_LIMIT,      EGC_ON_MEM_LIMIT )
  LROT_NUMENTRY ( ALWAYS,            EGC_ALWAYS )
LROT_END(node_egc, NULL, 0)


LROT_BEGIN(node_task)
  LROT_FUNCENTRY( post,            node_task_post )
  LROT_NUMENTRY ( LOW_PRIORITY,    TASK_PRIORITY_LOW )
  LROT_NUMENTRY ( MEDIUM_PRIORITY, TASK_PRIORITY_MEDIUM )
  LROT_NUMENTRY ( HIGH_PRIORITY,   TASK_PRIORITY_HIGH )
LROT_END(node_task, NULL, 0)


LROT_BEGIN(node)
  LROT_FUNCENTRY( chipid,     node_chipid )
  LROT_FUNCENTRY( compile,    node_compile )
  LROT_FUNCENTRY( dsleep,     node_dsleep )
  LROT_TABENTRY ( egc,        node_egc )
  LROT_FUNCENTRY( flashreload,luaN_reload_reboot )
  LROT_FUNCENTRY( flashindex, luaN_index )
  LROT_FUNCENTRY( heap,       node_heap )
  LROT_FUNCENTRY( info,       node_info )
  LROT_FUNCENTRY( input,      node_input )
  LROT_FUNCENTRY( output,     node_output )
  LROT_FUNCENTRY( osprint,    node_osprint )
  LROT_FUNCENTRY( restart,    node_restart )
  LROT_FUNCENTRY( stripdebug, node_stripdebug )
  LROT_TABENTRY ( task,       node_task )
  LROT_FUNCENTRY( uptime,     node_uptime )
LROT_END(node, NULL, 0)


NODEMCU_MODULE(NODE, "node", node, NULL);
