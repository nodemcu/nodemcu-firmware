// Common routines for handling serial input data

#include "serial_common.h"
#include "lauxlib.h"
#include <string.h>

// This is the historical max value
#define MAX_SERIAL_INPUT 255

struct serial_input_cfg {
  int receive_ref;
  int error_ref;
  char *line_buffer;
  size_t line_buffer_size;
  size_t line_position;
  uint16_t need_len;
  int16_t end_char;
};


static const char nostack[] = "out of stack";

static bool serial_input_invoke(int ref, const char *buf, size_t len)
{
  if(ref == LUA_NOREF || !buf || len == 0)
    return false;

  lua_State *L = lua_getstate();

  int top = lua_gettop(L);
  luaL_checkstack(L, 2, nostack);
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  lua_pushlstring(L, buf, len);
  luaL_pcallx(L, 1, 0);
  lua_settop(L, top);
  return true;
}


serial_input_cfg_t *serial_input_new(void)
{
  serial_input_cfg_t *cfg = calloc(1, sizeof(serial_input_cfg_t));
  if (!cfg)
    return NULL;
  cfg->receive_ref = cfg->error_ref = LUA_NOREF;
  cfg->end_char = -1;
  return cfg;
}


void serial_input_free(lua_State *L, serial_input_cfg_t *cfg)
{
  if (cfg->receive_ref != LUA_NOREF)
    luaL_unref(L, LUA_REGISTRYINDEX, cfg->receive_ref);
  if (cfg->error_ref != LUA_NOREF)
    luaL_unref(L, LUA_REGISTRYINDEX, cfg->error_ref);
  free(cfg->line_buffer);
  free(cfg);
}


bool serial_input_dispatch_data(serial_input_cfg_t *cfg, const char *buf, size_t len)
{
  if (!cfg)
    return false;
  else
    return serial_input_invoke(cfg->receive_ref, buf, len);
}


bool serial_input_report_error(serial_input_cfg_t *cfg, const char *buf, size_t len)
{
  if (!cfg)
    return false;
  else
    return serial_input_invoke(cfg->error_ref, buf, len);
}


void serial_input_feed_data(serial_input_cfg_t *cfg, const char *buf, size_t len)
{
  if (!cfg || !cfg->line_buffer || !buf || !len)
    return;

  const uint16_t need_len = cfg->need_len;
  const int16_t end_char = cfg->end_char;
  const size_t max_wanted =
    (end_char >= 0 && need_len == 0) ? cfg->line_buffer_size : need_len;

  for (unsigned i = 0; i < len; ++i)
  {
    char ch = buf[i];
    cfg->line_buffer[cfg->line_position] = ch;
    cfg->line_position++;

    bool at_end = (cfg->line_position >= max_wanted);
    bool end_char_found =
      (end_char >= 0 && (uint8_t)ch == (uint8_t)end_char);
    if (at_end || end_char_found) {
      // Reset line position early so callback can resize line_buffer if desired
      int n = cfg->line_position;
      cfg->line_position = 0;
      serial_input_dispatch_data(cfg, cfg->line_buffer, n);
    }
  }
}


bool serial_input_has_data_cb(serial_input_cfg_t *cfg)
{
  return cfg && cfg->receive_ref != LUA_NOREF;
}


// on("method", [number/char], function)
int serial_input_register(lua_State *L, serial_input_cfg_t *cfg)
{
  const char *method = luaL_checkstring(L, 1);
  const bool is_data = (strcmp(method, "data") == 0);
  const bool is_error = (strcmp(method, "error") == 0);
  if (!is_data && !is_error)
    return luaL_error(L, "method not supported");

  int fn_idx = -1;

  if (lua_isnumber(L, 2))
  {
    cfg->need_len = luaL_checkinteger(L, 2);
    cfg->end_char = -1;
  }
  else if (lua_isstring(L, 2))
  {
    size_t len;
    const char *end = luaL_checklstring(L, 2, &len);
    if (len != 1)
      return luaL_error(L, "only single byte end marker supported");
    cfg->need_len = 0;
    cfg->end_char = end[0];
  }
  else if (lua_isfunction(L, 2))
  {
    fn_idx = 2;
  }

  if (fn_idx == -1 && lua_isfunction(L, 3))
  {
    fn_idx = 3;
  }


  if (is_data)
  {
    if (cfg->receive_ref != LUA_NOREF)
      luaL_unref2(L, LUA_REGISTRYINDEX, cfg->receive_ref); // unref & clear

    if (fn_idx != -1) // Register and (re)alloc resources
    {
      luaL_checkstack(L, 1, nostack);
      lua_pushvalue(L, fn_idx);
      cfg->receive_ref = luaL_ref(L, LUA_REGISTRYINDEX);

      size_t min_size = (cfg->need_len > 0) ? cfg->need_len : MAX_SERIAL_INPUT;
      // Prevent dropping input; this should be an exceedingly rare condition
      if (cfg->line_position >= min_size)
        min_size = cfg->line_position + 1;

      if (cfg->line_buffer_size < min_size)
      {
        cfg->line_buffer = realloc(cfg->line_buffer, min_size);
        cfg->line_buffer_size = (cfg->line_buffer) ? min_size : 0;
        if (!cfg->line_buffer)
          return luaL_error(L, "out of mem");
      }
    }
    else // Free resources
    {
      free(cfg->line_buffer);
      cfg->line_buffer = NULL;
      cfg->line_buffer_size = 0;
      cfg->line_position = 0;
    }
  }
  else if (is_error)
  {
    if (cfg->error_ref != LUA_NOREF)
      luaL_unref2(L, LUA_REGISTRYINDEX, cfg->error_ref); // unref & clear

    if (fn_idx != -1)
    {
      luaL_checkstack(L, 1, nostack);
      lua_pushvalue(L, fn_idx);
      cfg->error_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
  }

  return 0;
}
