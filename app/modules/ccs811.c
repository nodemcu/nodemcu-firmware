/*
 * Driver for TI Texas Instruments CCS811 Temperature/Humidity Sensor.
 * Code By Metin KOC
 * Sixfab Inc. metin@sixfab.com
 * Code based on ADXL345 driver.
 */
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "user_interface.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gpio.h"

static const uint32_t ccs811_i2c_id = 0;

static const char metatable_name[] = "ccs811.device";
static const char unexpected_value[] = "unexpected value";
// Settings struct
typedef struct
{
  uint8_t i2c_addr;
  uint8_t wake_pin;
  uint8_t hw_id;
  uint8_t hw_version;
  uint16_t app_version;
} css_ctrl_ud_t;

#define IS_I2C_ADDR_VALID(addr) (((addr) == 0x5A) || ((addr) == 0x5B))

// The flags for errstat in ccs811_read()
// ERRSTAT is a merge of two hardware registers: ERROR_ID (bits 15-8) and STATUS (bits 7-0)
// Also bit 1 (which is always 0 in hardware) is set to 1 when an I2C read error occurs
#define CCS811_ERRSTAT_ERROR 0x0001             // There is an error, the ERROR_ID register (0xE0) contains the error source
#define CCS811_ERRSTAT_I2CFAIL 0x0002           // Bit flag added by software: I2C transaction error
#define CCS811_ERRSTAT_DATA_READY 0x0008        // A new data sample is ready in ALG_RESULT_DATA
#define CCS811_ERRSTAT_APP_VALID 0x0010         // Valid application firmware loaded
#define CCS811_ERRSTAT_FW_MODE 0x0080           // Firmware is in application mode (not boot mode)
#define CCS811_ERRSTAT_WRITE_REG_INVALID 0x0100 // The CCS811 received an I²C write request addressed to this station but with invalid register address ID
#define CCS811_ERRSTAT_READ_REG_INVALID 0x0200  // The CCS811 received an I²C read request to a mailbox ID that is invalid
#define CCS811_ERRSTAT_MEASMODE_INVALID 0x0400  // The CCS811 received an I²C request to write an unsupported mode to MEAS_MODE
#define CCS811_ERRSTAT_MAX_RESISTANCE 0x0800    // The sensor resistance measurement has reached or exceeded the maximum range
#define CCS811_ERRSTAT_HEATER_FAULT 0x1000      // The heater current in the CCS811 is not in range
#define CCS811_ERRSTAT_HEATER_SUPPLY 0x2000     // The heater voltage is not being applied correctly

// These flags should not be set. They flag errors.
#define CCS811_ERRSTAT_HWERRORS (CCS811_ERRSTAT_ERROR | CCS811_ERRSTAT_WRITE_REG_INVALID | CCS811_ERRSTAT_READ_REG_INVALID | CCS811_ERRSTAT_MEASMODE_INVALID | CCS811_ERRSTAT_MAX_RESISTANCE | CCS811_ERRSTAT_HEATER_FAULT | CCS811_ERRSTAT_HEATER_SUPPLY)
#define CCS811_ERRSTAT_ERRORS (CCS811_ERRSTAT_I2CFAIL | CCS811_ERRSTAT_HWERRORS)
// These flags should normally be set - after a measurement. They flag data available (and valid app running).
#define CCS811_ERRSTAT_OK (CCS811_ERRSTAT_DATA_READY | CCS811_ERRSTAT_APP_VALID | CCS811_ERRSTAT_FW_MODE)
// These flags could be set after a measurement. They flag data is not yet available (and valid app running).
#define CCS811_ERRSTAT_OK_NODATA (CCS811_ERRSTAT_APP_VALID | CCS811_ERRSTAT_FW_MODE)

// The values for mode in ccs811_start()
#define CCS811_MODE_IDLE 0
#define CCS811_MODE_1SEC 1
#define CCS811_MODE_10SEC 2
#define CCS811_MODE_60SEC 3

// Timings
#define CCS811_WAIT_AFTER_RESET_US 2000    // The CCS811 needs a wait after reset
#define CCS811_WAIT_AFTER_APPSTART_US 1000 // The CCS811 needs a wait after app start
#define CCS811_WAIT_AFTER_WAKE_US 50       // The CCS811 needs a wait after WAKE signal
#define CCS811_WAIT_AFTER_APPERASE_MS 500  // The CCS811 needs a wait after app erase (300ms from spec not enough)
#define CCS811_WAIT_AFTER_APPVERIFY_MS 70  // The CCS811 needs a wait after app verify
#define CCS811_WAIT_AFTER_APPDATA_MS 50    // The CCS811 needs a wait after writing app data

// CCS811 registers/mailboxes, all 1 byte except when stated otherwise
#define CCS811_STATUS 0x00
#define CCS811_MEAS_MODE 0x01
#define CCS811_ALG_RESULT_DATA 0x02 // up to 8 bytes
#define CCS811_RAW_DATA 0x03        // 2 bytes
#define CCS811_ENV_DATA 0x05        // 4 bytes
#define CCS811_THRESHOLDS 0x10      // 5 bytes
#define CCS811_BASELINE 0x11        // 2 bytes
#define CCS811_HW_ID 0x20
#define CCS811_HW_VERSION 0x21
#define CCS811_FW_BOOT_VERSION 0x23 // 2 bytes
#define CCS811_FW_APP_VERSION 0x24  // 2 bytes
#define CCS811_ERROR_ID 0xE0
#define CCS811_APP_ERASE 0xF1  // 4 bytes
#define CCS811_APP_DATA 0xF2   // 9 bytes
#define CCS811_APP_VERIFY 0xF3 // 0 bytes
#define CCS811_APP_START 0xF4  // 0 bytes
#define CCS811_SW_RESET 0xFF   // 4 bytes

#define NELEMS(x) (sizeof(x) / sizeof((x)[0]))

// Helper functions

// Set wake-up bin up to wake-up the processor
void wake_up(unsigned pin)
{
  platform_gpio_write(pin, PLATFORM_GPIO_LOW);
  os_delay_us(CCS811_WAIT_AFTER_WAKE_US);
}

// Set wake-up bin up 
void wake_down(unsigned pin)
{
  platform_gpio_write(pin, PLATFORM_GPIO_HIGH);
}

// Write to the register to the length of the buffer
static void write_reg(uint8_t ccs_addr, uint8_t reg, uint8_t *buf)
{
  platform_i2c_send_start(ccs811_i2c_id);
  platform_i2c_send_address(ccs811_i2c_id, ccs_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
  platform_i2c_send_byte(ccs811_i2c_id, reg);
  for (uint8_t i = 0; i < NELEMS(buf); i++)
  {
    platform_i2c_send_byte(ccs811_i2c_id, buf[i]);
  }
  platform_i2c_send_stop(ccs811_i2c_id);
}

// Read single byte from register
static uint8_t read_reg(uint8_t ccs_addr, uint8_t reg)
{
  uint8_t buf;
  platform_i2c_send_start(ccs811_i2c_id);
  platform_i2c_send_address(ccs811_i2c_id, ccs_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
  platform_i2c_send_byte(ccs811_i2c_id, reg);
  platform_i2c_send_stop(ccs811_i2c_id);
  //os_delay_us(2000);
  platform_i2c_send_start(ccs811_i2c_id);
  platform_i2c_send_address(ccs811_i2c_id, ccs_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
  buf = platform_i2c_recv_byte(ccs811_i2c_id, 0);
  platform_i2c_send_stop(ccs811_i2c_id);
  return buf;
}

// Send Application start command
static void write_app_start(uint8_t ccs_addr)
{
  uint16_t buf;
  platform_i2c_send_start(ccs811_i2c_id);
  platform_i2c_send_address(ccs811_i2c_id, ccs_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
  platform_i2c_send_byte(ccs811_i2c_id, CCS811_APP_START);
  platform_i2c_send_stop(ccs811_i2c_id);
  // Wait for app start
  os_delay_us(CCS811_WAIT_AFTER_APPSTART_US);
}

// Read double bytes to the length of the buffer
static void read_long(uint8_t ccs_addr, uint8_t reg, uint16_t *buf)
{
  platform_i2c_send_start(ccs811_i2c_id);
  platform_i2c_send_address(ccs811_i2c_id, ccs_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
  platform_i2c_send_byte(ccs811_i2c_id, reg);
  platform_i2c_send_stop(ccs811_i2c_id);
  //os_delay_us(2000);
  for (uint8_t i = 0; i < NELEMS(buf); i++)
  {
    platform_i2c_send_start(ccs811_i2c_id);
    platform_i2c_send_address(ccs811_i2c_id, ccs_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
    buf[i] = platform_i2c_recv_byte(ccs811_i2c_id, 1) << 8;
    buf[i] += platform_i2c_recv_byte(ccs811_i2c_id, 0);
    platform_i2c_send_stop(ccs811_i2c_id);
  }
}

// Read bytes to the length of the buffer
static void read_short(uint8_t ccs_addr, uint8_t reg, uint8_t *buf)
{
  platform_i2c_send_start(ccs811_i2c_id);
  platform_i2c_send_address(ccs811_i2c_id, ccs_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
  platform_i2c_send_byte(ccs811_i2c_id, reg);
  platform_i2c_send_stop(ccs811_i2c_id);
  platform_i2c_send_start(ccs811_i2c_id);
  platform_i2c_send_address(ccs811_i2c_id, ccs_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
  //os_delay_us(2000);
  for (uint8_t i = 0; i < NELEMS(buf); i++)
  {
    buf[i] = platform_i2c_recv_byte(ccs811_i2c_id, 1);
  }
  platform_i2c_send_stop(ccs811_i2c_id);
}

// Create CCS811 device
// Lua: ccs811.mode(i2c_id, i2c_addr, wakeup-pin)
static int ccs811_lua_register(lua_State *L)
{
  uint8_t sw_reset[] = {0x11, 0xE5, 0x72, 0x8A};
  uint8_t hw_id;
  uint8_t hw_version;
  uint16_t app_version[1];
  uint8_t status;

  // Verify the parameters
  uint8_t i2c_id = luaL_checkinteger(L, 1);
  luaL_argcheck(L, 0 == i2c_id, 1, "i2c_id must be 0");
  uint8_t i2c_addr = luaL_checkinteger(L, 2);
  luaL_argcheck(L, IS_I2C_ADDR_VALID(i2c_addr), 2, unexpected_value);

  // Get pin for watch trigger
  uint8_t wake_pin = luaL_checkinteger(L, 1);
  luaL_argcheck(L, platform_gpio_exists(wake_pin), 1, "Invalid pin");
  platform_gpio_mode(wake_pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);

  wake_up(wake_pin);
  status = read_reg(i2c_addr, CCS811_STATUS);
  wake_down(wake_pin);
  if (status == 0xFF)
  {
    return luaL_error(L, "found no device");
  }

  // Reset
  wake_up(wake_pin);
  write_reg(i2c_addr, CCS811_SW_RESET, sw_reset);

  os_delay_us(CCS811_WAIT_AFTER_RESET_US);

  // Check Status
  status = read_reg(i2c_addr, CCS811_STATUS);
  if (status != 0x10)
  {
    wake_down(wake_pin);
    return luaL_error(L, "ccs811:Not in boot mode, or no valid app: (%p)", status);
  }

  // Start App
  write_app_start(i2c_addr);

  // Check Hardware ID
  hw_id = read_reg(i2c_addr, CCS811_HW_ID);
  if (hw_id != 0x81)
  {
    wake_down(wake_pin);
    return luaL_error(L, "ccs811:Wrong Hardware ID (%p)", hw_id);
  }

  // Check Hardware version
  hw_version = read_reg(i2c_addr, CCS811_HW_VERSION);
  if ((hw_version & 0xF0) != 0x10)
  {
    wake_down(wake_pin);
    return luaL_error(L, "ccs811:Wrong Hardware Version (%p)", hw_version);
  }

  // Read application version
  read_long(i2c_addr, CCS811_FW_APP_VERSION, app_version);

  // Check if the switch was successful
  status = read_reg(i2c_addr, CCS811_STATUS);
  if (status != 0x90)
  {
    wake_down(wake_pin);
    return luaL_error(L, "ccs811:Not in app mode, or no valid app: (%p)", status);
  }

  wake_down(wake_pin);

  // Store configuration
  css_ctrl_ud_t *css_ctrl = lua_newuserdata(L, sizeof(css_ctrl_ud_t));
  luaL_getmetatable(L, metatable_name);
  lua_setmetatable(L, -2);
  css_ctrl->i2c_addr = i2c_addr;
  css_ctrl->wake_pin = wake_pin;
  css_ctrl->hw_id = hw_id;
  css_ctrl->hw_version = hw_version;
  css_ctrl->app_version = (app_version[0] << 8) + app_version[1];
  return 1;
}

// Setup CCS811 device
// Lua: ccs811.mode(MODE)
static int ccs811_lua_mode(lua_State *L)
{
  // Start initialisation
  uint8_t mode = luaL_checkinteger(L, 2);
  luaL_argcheck(L, (mode > 0) && (mode <= 3), 2, "Invalid mode");

  css_ctrl_ud_t *css_ctrl = luaL_checkudata(L, 1, metatable_name);
  wake_up(css_ctrl->wake_pin);
  uint8_t meas_mode[] = {(uint8_t)(mode << 4)};
  write_reg(css_ctrl->i2c_addr, CCS811_MEAS_MODE, meas_mode);
  wake_down(css_ctrl->wake_pin);

  return 0;
}

// Setup CCS811 device
// Lua: ccs811.read()
static int ccs811_lua_read(lua_State *L)
{
  uint8_t buf[8];
  bool ok;

  css_ctrl_ud_t *css_ctrl = luaL_checkudata(L, 1, metatable_name);

  wake_up(css_ctrl->wake_pin);

  // Read the results
  read_short(css_ctrl->i2c_addr, CCS811_ALG_RESULT_DATA, buf);
  uint16_t combined = buf[5] * 256 + buf[4];
  if (combined & ~(CCS811_ERRSTAT_HWERRORS | CCS811_ERRSTAT_OK))
    ok = false;                                            // Unused bits are 1: I2C transfer error
  combined &= CCS811_ERRSTAT_HWERRORS | CCS811_ERRSTAT_OK; // Clear all unused bits
  if (!ok)
    combined |= CCS811_ERRSTAT_I2CFAIL;
  // Clear ERROR_ID if flags are set
  if (combined & CCS811_ERRSTAT_HWERRORS)
  {
    uint8_t err = read_reg(css_ctrl->i2c_addr, CCS811_ERROR_ID);
    if (err == -1)
      combined |= CCS811_ERRSTAT_I2CFAIL; // Propagate I2C error
  }
  wake_down(css_ctrl->wake_pin);

  lua_pushnumber(L, (buf[0] << 8) + buf[1]);
  lua_pushnumber(L, (buf[2] << 8) + buf[3]);
  lua_pushnumber(L, combined);

  return 3;
}

// Retrieve error status
// Lua: css811:app_version()
static int ccs811_lua_app_version(lua_State *L)
{
  css_ctrl_ud_t *css_ctrl = luaL_checkudata(L, 1, metatable_name);
  lua_pushnumber(L, css_ctrl->app_version);
  return 1;
}

// Retrieve error status
// Lua: css811:error()
static int ccs811_lua_app_error(lua_State *L)
{
  uint8_t error;
  css_ctrl_ud_t *css_ctrl = luaL_checkudata(L, 1, metatable_name);

  wake_up(css_ctrl->wake_pin);

  // Read the results
  error = read_reg(css_ctrl->i2c_addr, CCS811_ERROR_ID);
  wake_down(css_ctrl->wake_pin);
  lua_pushnumber(L, error);
  return 1;
}


LROT_BEGIN(ccs811, NULL, 0)
LROT_FUNCENTRY(create, ccs811_lua_register)
LROT_NUMENTRY(MODE_IDLE, CCS811_MODE_IDLE)
LROT_NUMENTRY(MODE_1SEC, CCS811_MODE_1SEC)
LROT_NUMENTRY(MODE_10SEC, CCS811_MODE_10SEC)
LROT_NUMENTRY(MODE_60SEC, CCS811_MODE_60SEC)
LROT_END(ccs811, NULL, 0)

LROT_BEGIN(ccs811_instance, NULL, LROT_MASK_GC_INDEX)
LROT_TABENTRY(__index, ccs811_instance)
LROT_FUNCENTRY(mode, ccs811_lua_mode)
LROT_FUNCENTRY(read, ccs811_lua_read)
LROT_FUNCENTRY(app_version, ccs811_lua_app_version)
LROT_FUNCENTRY(error, ccs811_lua_app_error)
LROT_END(ccs811_instance, NULL, LROT_MASK_GC_INDEX)

int luaopen_ccs811(lua_State *L)
{
  luaL_rometatable(L, metatable_name, LROT_TABLEREF(ccs811_instance));
  return 0;
}

NODEMCU_MODULE(CCS811, "ccs811", ccs811, luaopen_ccs811);
