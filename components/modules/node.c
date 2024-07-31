#include "module.h"
#include "lauxlib.h"
#include "common.h"
#include "lundump.h"
#include "platform.h"
#include "task/task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "soc/efuse_reg.h"
#include "ldebug.h"
#include "esp_vfs.h"
#include "lnodeaux.h"
#include "lpanic.h"
#include "rom/rtc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "user_version.h"

static void restart_callback(TimerHandle_t timer) {
  (void)timer;
  esp_restart();
}

static int default_onerror(lua_State *L) {
  /* Use Lua print to print the ToS */
  lua_settop(L, 1);
  lua_getglobal(L, "print");
  lua_insert(L, 1);
  lua_pcall(L, 1, 0, 0);
  /* One first time through set automatic restart after 2s delay */
  static TimerHandle_t restart_timer;
  if (!restart_timer) {
    restart_timer = xTimerCreate(
        "error_restart", pdMS_TO_TICKS(2000), pdFALSE, NULL, restart_callback);
    if (xTimerStart(restart_timer, portMAX_DELAY) != pdPASS)
      esp_restart(); // should never happen, but Justin Case fallback
  }
  return 0;
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


// Lua: node.bootreason()
static int node_bootreason( lua_State *L)
{
  int panicval = panic_get_nvval();
  RESET_REASON rr0 = rtc_get_reset_reason(0);
  unsigned rawinfo = 3;
  // rawinfo can take these values as defined in docs/modules/node.md
  //
  // 1, power-on
  // 2, reset (software?)
  // 3, hardware reset via reset pin or unknown reason
  // 4, WDT reset (watchdog timeout)
  //
  // extendedinfo can take these values as definded in docs/modules/node.md
  //
  // 0, power-on
  // 1, hardware watchdog reset
  // 2, exception reset
  // 3, software watchdog reset
  // 4, software restart
  // 5, wake from deep sleep
  // 6, external reset
  // added values from rom/rtc.h with offset 7
  // 7: NO_MEAN                =  0,
  // 8: POWERON_RESET          =  1,    /**<1, Vbat power on reset*/
  // 9:
  // 10: SW_RESET               =  3,    /**<3, Software reset digital core*/
  // 11: OWDT_RESET             =  4,    /**<4, Legacy watch dog reset digital core*/
  // 12: DEEPSLEEP_RESET        =  5,    /**<3, Deep Sleep reset digital core*/
  // 13: SDIO_RESET             =  6,    /**<6, Reset by SLC module, reset digital core*/
  // 14: TG0WDT_SYS_RESET       =  7,    /**<7, Timer Group0 Watch dog reset digital core*/
  // 15: TG1WDT_SYS_RESET       =  8,    /**<8, Timer Group1 Watch dog reset digital core*/
  // 16: RTCWDT_SYS_RESET       =  9,    /**<9, RTC Watch dog Reset digital core*/
  // 17: INTRUSION_RESET        = 10,    /**<10, Instrusion tested to reset CPU*/
  // 18: TGWDT_CPU_RESET        = 11,    /**<11, Time Group reset CPU*/
  // 19: SW_CPU_RESET           = 12,    /**<12, Software reset CPU*/
  // 20: RTCWDT_CPU_RESET       = 13,    /**<13, RTC Watch dog Reset CPU*/
  // 21: EXT_CPU_RESET          = 14,    /**<14, for APP CPU, reseted by PRO CPU*/
  // 22: RTCWDT_BROWN_OUT_RESET = 15,    /**<15, Reset when the vdd voltage is not stable*/
  // 23: RTCWDT_RTC_RESET       = 16     /**<16, RTC Watch dog reset digital core and rtc module*/`
#if !defined(CONFIG_IDF_TARGET_ESP32)
# define SW_CPU_RESET   RTC_SW_CPU_RESET
# define SW_RESET       RTC_SW_SYS_RESET
#endif
  switch (rr0) {
    case POWERON_RESET:
      rawinfo = 1; break;
    case SW_CPU_RESET:
    case SW_RESET:
      rawinfo = 2; break;
    case NO_MEAN:
#if defined(CONFIG_IDF_TARGET_ESP32)
    case EXT_CPU_RESET:
#endif
    case DEEPSLEEP_RESET:
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32C6)
    case SDIO_RESET:
#endif
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32H2)
    case GLITCH_RTC_RESET:
#endif
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    case EFUSE_RESET:
#endif
#if defined(CONFIG_IDF_TARGET_ESP32C6)
    case JTAG_RESET:
#endif
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    case USB_UART_CHIP_RESET:
    case USB_JTAG_CHIP_RESET:
#endif
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32H2)
    case POWER_GLITCH_RESET:
#endif
    case TG0WDT_SYS_RESET:
    case TG1WDT_SYS_RESET:
#if !defined(CONFIG_IDF_TARGET_ESP32C6)
    case INTRUSION_RESET:
#endif
    case RTCWDT_BROWN_OUT_RESET:
    case RTCWDT_RTC_RESET:
      rawinfo = 3; break;
#if defined(CONFIG_IDF_TARGET_ESP32)
    case OWDT_RESET:
    case TGWDT_CPU_RESET:
#else
    case TG0WDT_CPU_RESET:
    case TG1WDT_CPU_RESET:
    case SUPER_WDT_RESET:
#endif
    case RTCWDT_CPU_RESET:
    case RTCWDT_SYS_RESET:
      rawinfo = 4; break;
  }
  lua_pushinteger(L, (lua_Integer)rawinfo);
  lua_pushinteger(L, (lua_Integer)rr0+7);
  if (rr0 == SW_CPU_RESET) {
    lua_pushinteger(L, (lua_Integer)panicval);
    return 3;
  }
  return 2;
}

#if defined(CONFIG_IDF_TARGET_ESP32)
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
#endif

// Lua: node.chipmodel()
static int node_chipmodel(lua_State *L)
{
  lua_pushstring(L, CONFIG_IDF_TARGET);
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
   panic_clear_nvval();
   esp_restart ();
   return 0;
}

static void node_sleep_set_uart (lua_State *L, int uart)
{
  int err = esp_sleep_enable_uart_wakeup(uart);
  if (err) {
    luaL_error(L, "Error %d returned from esp_sleep_enable_uart_wakeup(%d)", err, uart);
  }
}

static bool node_sleep_get_time_options (lua_State *L, int64_t *usecs)
{
  lua_getfield(L, 1, "us");
  lua_getfield(L, 1, "secs");
  bool option_present = !lua_isnil(L, 2) || !lua_isnil(L, 3);
  lua_pop(L, 2);
  *usecs = 0;
  if (option_present) {
    *usecs += opt_checkint(L, "us", 0);
    *usecs += (int64_t)opt_checkint(L, "secs", 0) * 1000000;
  }
  return option_present;
}

static void node_sleep_disable_wakeup_sources (lua_State *L)
{
  // Start with known state, to ensure previous sleep calls don't leave any
  // settings left over
  int err = esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (err) {
    luaL_error(L, "Error %d returned from esp_sleep_disable_wakeup_source", err);
  }
}

static int node_sleep (lua_State *L)
{
  lua_settop(L, 1);
  luaL_checktable(L, 1);
  node_sleep_disable_wakeup_sources(L);

  // uart options: uart = num|{num, num, ...}
  lua_getfield(L, -1, "uart");
  int type = lua_type(L, -1);
  if (type == LUA_TNUMBER) {
    node_sleep_set_uart(L, lua_tointeger(L, -1));
  } else if (type == LUA_TTABLE) {
    for (int i = 1; ; i++) {
      lua_rawgeti(L, -1, i);
      if (lua_isnil(L, -1)) {
        lua_pop(L, 1); // uart[i]
        break;
      }
      int uart = lua_tointeger(L, -1);
      lua_pop(L, 1); // uart[i]
      node_sleep_set_uart(L, uart);
    }
  } else if (type != LUA_TNIL) {
    return opt_error(L, "uart", "must be integer or table");
  }
  lua_pop(L, 1); // uart

  // gpio option: boolean (individual pins are configured in advance with gpio.wakeup())

  // Make sure to do GPIO before touch, because esp_sleep_enable_gpio_wakeup()
  // seems to think touch is not compatible with GPIO wakeup and will error the
  // call if you order them the other way round, despite the fact that
  // esp_sleep_enable_touchpad_wakeup() does not have a similar check, and I've
  // tested using both GPIO and touch wakeups at once and it works fine for me.
  // I think this is simply a bug in the Espressif SDK, because sleep_modes.rst
  // only mentions compatibility issues with touch and EXT0 wakeup, which is
  // not the same as GPIO wakeup.
  if (opt_checkbool(L, "gpio", false)) {
    int err = esp_sleep_enable_gpio_wakeup();
    if (err) {
      return luaL_error(L, "Error %d returned from esp_sleep_enable_gpio_wakeup()", err);
    }
  }

  // time options: us, secs
  int64_t usecs = 0;
  if (node_sleep_get_time_options(L, &usecs)) {
    esp_sleep_enable_timer_wakeup(usecs);
  }

#if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2)
  // touch option: boolean
  if (opt_checkbool(L, "touch", false)) {
    int err = esp_sleep_enable_touchpad_wakeup();
    if (err) {
      return luaL_error(L, "Error %d returned from esp_sleep_enable_touchpad_wakeup()", err);
    }
  }

  // ulp option: boolean
  if (opt_checkbool(L, "ulp", false)) {
    int err = esp_sleep_enable_ulp_wakeup();
    if (err) {
      return luaL_error(L, "Error %d returned from esp_sleep_enable_ulp_wakeup()", err);
    }
  }
#endif

  int err = esp_light_sleep_start();
  if (err == ESP_ERR_INVALID_STATE) {
    return luaL_error(L, "WiFi and BT must be stopped before sleeping");
  } else if (err) {
    return luaL_error(L, "Error %d returned from esp_light_sleep_start()", err);
  }

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  lua_pushinteger(L, (int)cause);
  return 1;
}


// Lua: node.dsleep (microseconds|{opts})
static int node_dsleep (lua_State *L)
{
  lua_settop(L, 1);
  node_sleep_disable_wakeup_sources(L);
  bool enable_timer_wakeup = false;
  int64_t usecs = 0;
  int type = lua_type(L, 1);
  if (type == LUA_TNUMBER) {
    enable_timer_wakeup = true;
    usecs = lua_tointeger(L, 1);
  } else if (type == LUA_TTABLE) {
    enable_timer_wakeup = node_sleep_get_time_options(L, &usecs);

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

#if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2)
    bool pull = opt_checkbool(L, "pull", false);
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

    int level = opt_checkint_range(L, "level", 1, 0, 1);
    if (pin_mask) {
      esp_sleep_ext1_wakeup_mode_t mode = (level == 1) ?
        ESP_EXT1_WAKEUP_ANY_HIGH :
#ifdef CONFIG_IDF_TARGET_ESP32
        ESP_EXT1_WAKEUP_ALL_LOW;
#else
        ESP_EXT1_WAKEUP_ANY_LOW;
#endif
      int err = esp_sleep_enable_ext1_wakeup(pin_mask, mode);
      if (err) {
        return luaL_error(L, "Error %d returned from esp_sleep_enable_ext1_wakeup", err);
      }
    }

    bool touch = opt_checkbool(L, "touch", false);
    if (touch) {
      esp_sleep_enable_touchpad_wakeup();
    }
#endif

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

static void get_lfs_config ( lua_State* );

static int node_info( lua_State* L ){
  const char* options[] = {"lfs", "hw", "sw_version", "build_config", "default", NULL};
  int option = luaL_checkoption (L, 1, options[4], options);

  switch (option) {
    case 0: { // lfs
      get_lfs_config(L);
      return 1;
    }
    case 1: { // hw
      uint32_t flash_size, flash_id;
      esp_chip_info_t chip_info;
      esp_chip_info(&chip_info);
      if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ESP_LOGW("node", "Get flash size failed");
        flash_size = 0;
      }
      if(esp_flash_read_id(NULL, &flash_id) != ESP_OK) {
        ESP_LOGW("node", "Get flash ID failed");
        flash_id = 0;
      }
      lua_createtable(L, 0, 7);
      add_string_field(L, CONFIG_IDF_TARGET,     "chip_model");
      add_int_field(L, chip_info.features,       "chip_features");
      add_int_field(L, chip_info.revision / 100, "chip_major_rev");
      add_int_field(L, chip_info.revision % 100, "chip_minor_rev");
      add_int_field(L, chip_info.cores,          "cpu_cores");
      add_int_field(L, flash_size / 1024,        "flash_size"); // flash size in KB
      add_int_field(L, flash_id,                 "flash_id");
      return 1;
    }
    // based on PR https://github.com/nodemcu/nodemcu-firmware/pull/3289
    case 2: { // sw_version
      lua_createtable(L, 0, 8);
      add_string_field(L, NODE_VERSION,          "node_version");
      add_int_field(L,    NODE_VERSION_MAJOR,    "node_version_major");
      add_int_field(L,    NODE_VERSION_MINOR,    "node_version_minor");
      add_int_field(L,    NODE_VERSION_REVISION, "node_version_revision");
      add_string_field(L, BUILDINFO_BRANCH,      "git_branch");
      add_string_field(L, BUILDINFO_COMMIT_ID,   "git_commit_id");
      add_string_field(L, BUILDINFO_RELEASE,     "git_release");
      add_string_field(L, BUILDINFO_RELEASE_DTS, "git_commit_dts");
      return 1;
    }
      case 3: { // build_config
      lua_createtable(L, 0, 5);
      lua_pushboolean(L, CONFIG_MBEDTLS_TLS_ENABLED);
      lua_setfield(L, -2,                       "ssl");
      add_int_field(L,    BUILDINFO_LFS_SIZE,   "lfs_size");
      add_string_field(L, BUILDINFO_BUILD_TYPE, "number_type");
      add_string_field(L, BUILDINFO_MODULES,    "modules");
      #if CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
        add_string_field(L, "uart", "esp_console");
      #elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
        add_string_field(L, "usb_serial_jtag", "esp_console");
      #elif CONFIG_ESP_CONSOLE_USB_CDC
        add_string_field(L, "usb_cdc", "esp_console");
      #endif
      return 1;
    }
    default: {  // default
      lua_newtable(L);
      return 1;
    }
  }
}

// Lua: input("string")
static int node_input( lua_State* L )
{
  size_t l = 0;
  const char *s = luaL_checklstring(L, 1, &l);
  if (l > 0 && l < LUA_MAXINPUT - 1)
    lua_input_string(s, l);
  return 0;
}

// The implementation of node.output implies replacing stdout with a virtual write-only file of
// which we can capture fwrite calls.
// When there is any write to the replaced stdout, our function redir_write will be called.
// we can then invoke the lua callback.

// A buffer size that should be sufficient for most cases, yet not so large
// as to present an issue.
# define OUTPUT_CHUNK_SIZE 127
typedef struct {
  uint8_t used;
  uint8_t bytes[OUTPUT_CHUNK_SIZE];
} output_chunk_t;

static task_handle_t output_task;    // for getting output into the LVM thread
static lua_ref_t output_redir = LUA_NOREF;  // this will hold the Lua callback
static FILE *serial_debug;                  // the console uart, if wanted
static const char *VFS_REDIR = "/redir";    // virtual filesystem mount point

// redir_write will be called everytime any code writes to stdout when
// redirection is active, from ANY RTOS thread
ssize_t redir_write(int fd, const void *data, size_t size) {
  UNUSED(fd);
  if (size)
  {
    size_t n = (size > OUTPUT_CHUNK_SIZE) ? OUTPUT_CHUNK_SIZE : size;
    output_chunk_t *chunk = malloc(sizeof(output_chunk_t));
    chunk->used = (uint8_t)n;
    memcpy(chunk->bytes, data, n);
    _Static_assert(sizeof(task_param_t) >= sizeof(chunk), "cast error below");
    if (!task_post_high(output_task, (task_param_t)chunk))
    {
      static const char overflow[] = "E: output overflow\n";
      fwrite(overflow, sizeof(overflow) -1, sizeof(char), serial_debug);
      free(chunk);
      return -1;
    }

    if (serial_debug)
    {
      size_t written = 0;
      while (written < n)
      {
        size_t w = fwrite(
          data + written, sizeof(char), n - written, serial_debug);
        if (w > 0)
          written += w;
        else break;
      }
    }

    return n;
  }
  else
    return 0;
}

void redir_output(task_param_t param, task_prio_t prio)
{
  UNUSED(prio);
  output_chunk_t *chunk = (output_chunk_t *)param;
  bool redir_active = (output_redir != LUA_NOREF);
  if (redir_active)
  {
    lua_State *L = lua_getstate();
    lua_rawgeti(L, LUA_REGISTRYINDEX, output_redir);
    lua_pushlstring(L, (char *)chunk->bytes, chunk->used);
    luaL_pcallx(L, 1, 0);
  }
  free(chunk);
}

#if !defined(CONFIG_ESP_CONSOLE_NONE)
static const char *default_console_name(void)
{
  return
#if defined(CONFIG_ESP_CONSOLE_UART)
# define STRINGIFY(x) STRINGIFY2(x)
# define STRINGIFY2(x) #x
    "/dev/uart/" STRINGIFY(CONFIG_ESP_CONSOLE_UART_NUM);
#undef STRINGIFY2
#undef STRINGIFY
#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    "/dev/cdcacm";
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    "/dev/usbserjtag";
#endif
}
#endif

// redir_open is called when fopen() is called on /redir/xxx
int redir_open(const char *path, int flags, int mode) {
    return 79;  // since we only have one "file", just return some fd number to make the VFS system happy
}

// Lua: node.output(func, serial_debug)
static int node_output(lua_State *L) {
    if (serial_debug)
    {
        fclose(serial_debug);
        serial_debug = NULL;
    }
    if (lua_isfunction(L, 1)) {
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
            freopen(VFS_REDIR, "w", stdout);

            if (lua_isnoneornil(L, 2) ||
                (lua_isnumber(L, 2) && lua_tonumber(L, 2)))
                    serial_debug = fopen(default_console_name(), "w");
        } else {
            luaX_unset_ref(L, &output_redir);  // dereference previous callback
        }
        luaX_set_ref(L, 1, &output_redir);  // set the callback
    } else {
        if (output_redir != LUA_NOREF) {
#if defined(CONFIG_ESP_CONSOLE_NONE)
            fclose(stdout);
#else
            // reopen the console device onto the stdout stream
            freopen(default_console_name(), "w", stdout);
#endif
            ESP_ERROR_CHECK(esp_vfs_unregister(VFS_REDIR));
            luaX_unset_ref(L, &output_redir);
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


#if defined(CONFIG_LUA_VERSION_51)
// Lua: node.egc.setmode( mode, [param])
// where the mode is one of the node.egc constants  NOT_ACTIVE , ON_ALLOC_FAILURE,
// ON_MEM_LIMIT, ALWAYS.  In the case of ON_MEM_LIMIT an integer parameter is reqired
// See legc.h and lecg.c.
static int node_egc_setmode(lua_State* L) {
  unsigned mode  = luaL_checkinteger(L, 1);
  unsigned limit = luaL_optinteger (L, 2, 0);

  luaL_argcheck(L, mode <= (EGC_ON_ALLOC_FAILURE | EGC_ON_MEM_LIMIT | EGC_ALWAYS), 1, "invalid mode");
  luaL_argcheck(L, !(mode & EGC_ON_MEM_LIMIT) || limit>0, 1, "limit must be non-zero");

  lua_setegcmode( L, mode, limit );
  return 0;
}
#endif


static int writer(lua_State* L, const void* p, size_t size, void* u)
{
  UNUSED(L);
  FILE *file = (FILE *)u;
  if (!file)
    return 1;

  if (size != 0 && (fwrite((const char *)p, size, 1, file) != 1) )
    return 1;

  return 0;
}


#define toproto(L,i) (clvalue(L->top+(i))->l.p)
// Lua: compile(filename) -- compile lua file into lua bytecode, and save to .lc
static int node_compile( lua_State* L )
{
  Proto* f;
  FILE *file = 0;
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );

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

  f = toproto(L, -1);

  int stripping = 1;      /* strip debug information? */

  file = fopen(output, "w+");
  if (!file)
  {
    luaM_free( L, output );
    return luaL_error(L, "cannot open/write to file");
  }

  lua_lock(L);
  int result = luaU_dump(L, f, writer, file, stripping);
  lua_unlock(L);

  if (fflush(file) != 0) {
    // overwrite Lua error, like writer() does in case of a file io error
    result = 1;
  }
  fclose(file);
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
  luaL_pcallx(L, 1, 0);
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
  luaL_argcheck(L, Ltype == LUA_TFUNCTION, n, "invalid function");
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


// Lua: n = node.LFS.reload(lfsimage)
static int node_lfsreload (lua_State *L) {
  lua_settop(L, 1);
  luaL_lfsreload(L);
  return 1;
}

// Lua: n = node.flashreload(lfsimage)
static int node_lfsreload_deprecated (lua_State *L) {
  platform_print_deprecation_note("node.flashreload", "soon. Use node.LFS interface instead");
  return node_lfsreload (L);
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

static void get_lfs_config ( lua_State* L ){
    int config[5];
    lua_getlfsconfig(L, config);
    lua_createtable(L, 0, 4);
    add_int_field(L, config[0], "lfs_mapped");
    add_int_field(L, config[1], "lfs_base");
    add_int_field(L, config[2], "lfs_size");
    add_int_field(L, config[3], "lfs_used");
}

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





#if defined(CONFIG_LUA_VERSION_51)
LROT_BEGIN(node_egc, NULL, 0)
  LROT_FUNCENTRY( setmode,           node_egc_setmode )
  LROT_NUMENTRY ( NOT_ACTIVE,        EGC_NOT_ACTIVE )
  LROT_NUMENTRY ( ON_ALLOC_FAILURE,  EGC_ON_ALLOC_FAILURE )
  LROT_NUMENTRY ( ON_MEM_LIMIT,      EGC_ON_MEM_LIMIT )
  LROT_NUMENTRY ( ALWAYS,            EGC_ALWAYS )
LROT_END(node_egc, NULL, 0)
#endif


LROT_BEGIN(node_task, NULL, 0)
  LROT_FUNCENTRY( post,            node_task_post )
  LROT_NUMENTRY ( LOW_PRIORITY,    TASK_PRIORITY_LOW )
  LROT_NUMENTRY ( MEDIUM_PRIORITY, TASK_PRIORITY_MEDIUM )
  LROT_NUMENTRY ( HIGH_PRIORITY,   TASK_PRIORITY_HIGH )
LROT_END(node_task, NULL, 0)


// Wakup reasons
LROT_BEGIN(node_wakeup, NULL, 0)
  LROT_NUMENTRY ( GPIO,     ESP_SLEEP_WAKEUP_GPIO )
  LROT_NUMENTRY ( TIMER,    ESP_SLEEP_WAKEUP_TIMER )
  LROT_NUMENTRY ( TOUCHPAD, ESP_SLEEP_WAKEUP_TOUCHPAD )
  LROT_NUMENTRY ( UART,     ESP_SLEEP_WAKEUP_UART )
  LROT_NUMENTRY ( ULP,      ESP_SLEEP_WAKEUP_ULP )
LROT_END(node_wakeup, NULL, 0)

LROT_BEGIN(node, NULL, 0)
  LROT_FUNCENTRY( bootreason, node_bootreason )
#if defined(CONFIG_IDF_TARGET_ESP32)
  LROT_FUNCENTRY( chipid,     node_chipid )
#endif
  LROT_FUNCENTRY( chipmodel,  node_chipmodel )
  LROT_FUNCENTRY( compile,    node_compile )
  LROT_FUNCENTRY( dsleep,     node_dsleep )
#if defined(CONFIG_LUA_VERSION_51)
  LROT_TABENTRY ( egc,        node_egc )
#endif
  LROT_FUNCENTRY( flashreload,node_lfsreload_deprecated )
  LROT_FUNCENTRY( flashindex, node_lfsindex )
  LROT_TABENTRY(  LFS,        node_lfs )
  LROT_FUNCENTRY( heap,       node_heap )
  LROT_FUNCENTRY( info,       node_info )
  LROT_FUNCENTRY( input,      node_input )
  LROT_FUNCENTRY( output,     node_output )
  LROT_FUNCENTRY( osprint,    node_osprint )
  LROT_FUNCENTRY( restart,    node_restart )
  LROT_FUNCENTRY( setonerror, node_setonerror )
  LROT_FUNCENTRY( sleep,      node_sleep )
  LROT_FUNCENTRY( stripdebug, node_stripdebug )
  LROT_TABENTRY ( task,       node_task )
  LROT_FUNCENTRY( uptime,     node_uptime )
  LROT_TABENTRY ( wakeup,     node_wakeup )
LROT_END(node, NULL, 0)

int luaopen_node(lua_State *L)
{
  output_task = task_get_id(redir_output);

  lua_settop(L, 0);
  return node_setonerror(L);  /* set default onerror action */
}

NODEMCU_MODULE(NODE, "node", node, luaopen_node);
