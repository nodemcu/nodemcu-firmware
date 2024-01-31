#include "module.h"
#include "lauxlib.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include "esp_heap_trace.h"
#pragma GCC diagnostic pop

static heap_trace_record_t *buffer = NULL;

static int lht_init(lua_State *L)
{
  int records = luaL_checkint(L, 1);
  if (records <= 0)
    return luaL_error(L, "invalid trace buffer size");

  heap_trace_stop();
  free(buffer);
  buffer = calloc(sizeof(heap_trace_record_t), records);
  if (!buffer)
    return luaL_error(L, "out of memory");

  esp_err_t err = heap_trace_init_standalone(buffer, records);
  if (err != ESP_OK)
    return luaL_error(L, "failed to init heap tracing; code %d", err);

  return 0;
}


static int lht_start(lua_State *L)
{
  esp_err_t err;
  switch(luaL_checkint(L, 1))
  {
    case HEAP_TRACE_ALL:   err = heap_trace_start(HEAP_TRACE_ALL);   break;
    case HEAP_TRACE_LEAKS: err = heap_trace_start(HEAP_TRACE_LEAKS); break;
    default: return luaL_error(L, "invalid trace mode");
  }

  if (err != ESP_OK)
    return luaL_error(L, "failed to start heap tracing; code %d", err);

  return 0;
}


static int lht_stop(lua_State *L)
{
  esp_err_t err = heap_trace_stop();
  if (err != ESP_OK)
    return luaL_error(L, "failed to stop heap tracing; code %d", err);

  return 0;
}


static int lht_resume(lua_State *L)
{
  esp_err_t err = heap_trace_resume();
  if (err != ESP_OK)
    return luaL_error(L, "failed to resume heap tracing; code %d", err);

  return 0;
}


static int lht_dump(lua_State *L)
{
  (void)L;
  heap_trace_dump();
  return 0;
}


LROT_BEGIN(heaptrace, NULL, 0)
  LROT_FUNCENTRY( init,       lht_init )
  LROT_FUNCENTRY( start,      lht_start )
  LROT_FUNCENTRY( stop,       lht_stop )
  LROT_FUNCENTRY( resume,     lht_resume )
  LROT_FUNCENTRY( dump,       lht_dump )

  LROT_NUMENTRY( TRACE_ALL,   HEAP_TRACE_ALL )
  LROT_NUMENTRY( TRACE_LEAKS, HEAP_TRACE_LEAKS )
LROT_END(heaptrace, NULL, 0)

NODEMCU_MODULE(HEAPTRACE, "heaptrace", heaptrace, NULL);
