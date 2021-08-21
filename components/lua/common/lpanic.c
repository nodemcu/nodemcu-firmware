#include "lpanic.h"
#include "lauxlib.h"
#include <stdint.h>
#include <stdlib.h>
#ifndef LUA_CROSS_COMPILER
#include "esp_system.h"
/* defined in esp_system_internal.h */
void esp_reset_reason_set_hint(esp_reset_reason_t hint);
#endif


#ifndef LUA_CROSS_COMPILER
/*
 * upper 28 bit have magic value 0xD1EC0DE0
 * if paniclevel is valid (i.e. matches magic value)
 * lower 4 bits have paniclevel:
 * 0 = no panic occurred
 * 1..15 = one..fifteen subsequent panic(s) occurred
 */
static __NOINIT_ATTR uint32_t l_rtc_panic_val;

void panic_clear_nvval() {
  l_rtc_panic_val = 0xD1EC0DE0;
}

int panic_get_nvval() {
  if ((l_rtc_panic_val & 0xfffffff0) == 0xD1EC0DE0) {
    return (l_rtc_panic_val & 0xf);
  }
  panic_clear_nvval();
  return 0;
}
#endif


int panic (lua_State *L) {
  (void)L;  /* to avoid warnings */
#ifndef LUA_CROSS_COMPILER
  uint8_t paniclevel = panic_get_nvval();
  if (paniclevel < 15) paniclevel++;
  l_rtc_panic_val = 0xD1EC0DE0 | paniclevel;
#endif
  lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n",
                   lua_tostring(L, -1));
#ifndef LUA_CROSS_COMPILER
  /* call abort() directly - we don't want another reset cause to intervene */
  esp_reset_reason_set_hint(ESP_RST_PANIC);
#endif
  abort();
  while (1) {}
  return 0;
}
