#include "module.h"
#include "lauxlib.h"
#include "legc.h"
#include "lundump.h"
#include "platform.h"
#include "task/task.h"
#include "vfs.h"
#include "esp_system.h"
#include "esp_log.h"
#include "soc/efuse_reg.h"
#include "ldebug.h"

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


// Lua: node.dsleep (microseconds)
static int node_dsleep (lua_State *L)
{
  uint64_t us = luaL_optinteger (L, 1, 0);
  esp_deep_sleep (us);
  return 0;
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


static const LUA_REG_TYPE node_egc_map[] = {
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
  { LSTRKEY( "chipid" ),	LFUNCVAL( node_chipid )	},
  { LSTRKEY( "compile" ),	LFUNCVAL( node_compile )	},
  { LSTRKEY( "dsleep" ),    LFUNCVAL( node_dsleep ) 	},
  { LSTRKEY( "egc" ),		LROVAL( node_egc_map )  	},
  { LSTRKEY( "heap" ),      LFUNCVAL( node_heap )   	},
  { LSTRKEY( "input" ),		LFUNCVAL( node_input )		},
  { LSTRKEY( "osprint" ),   LFUNCVAL( node_osprint )    },
  { LSTRKEY( "restart" ),   LFUNCVAL( node_restart )	},
  { LSTRKEY( "stripdebug"), LFUNCVAL( node_stripdebug )	},
  { LSTRKEY( "task" ),      LROVAL( node_task_map ) 	},
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(NODE, "node", node_map, NULL);
