//***************************************************************************
// Si7021 module for ESP8266 with nodeMCU
// fetchbot @github
// MIT license, http://opensource.org/licenses/MIT
//***************************************************************************

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "osapi.h"
#include "c_stdlib.h"


//***************************************************************************
// CHIP
//***************************************************************************
#define ADS1115_ADS1015 ( 15)
#define ADS1115_ADS1115 (115)

//***************************************************************************
// I2C ADDRESS DEFINITON
//***************************************************************************

#define ADS1115_I2C_ADDR_GND        (0x48)
#define ADS1115_I2C_ADDR_VDD        (0x49)
#define ADS1115_I2C_ADDR_SDA        (0x4A)
#define ADS1115_I2C_ADDR_SCL        (0x4B)
#define IS_I2C_ADDR_VALID(addr) (((addr) & 0xFC) == 0x48)

//***************************************************************************
// POINTER REGISTER
//***************************************************************************

#define ADS1115_POINTER_MASK        (0x03)
#define ADS1115_POINTER_CONVERSION  (0x00)
#define ADS1115_POINTER_CONFIG      (0x01)
#define ADS1115_POINTER_THRESH_LOW  (0x02)
#define ADS1115_POINTER_THRESH_HI   (0x03)

//***************************************************************************
// CONFIG REGISTER
//***************************************************************************

#define ADS1115_OS_MASK             (0x8000)
#define ADS1115_OS_NON              (0x0000)
#define ADS1115_OS_SINGLE           (0x8000)    // Write: Set to start a single-conversion
#define ADS1115_OS_BUSY             (0x0000)    // Read: Bit = 0 when conversion is in progress
#define ADS1115_OS_NOTBUSY          (0x8000)    // Read: Bit = 1 when device is not performing a conversion

#define ADS1115_MUX_MASK            (0x7000)
#define ADS1115_MUX_DIFF_0_1        (0x0000)    // Differential P = AIN0, N = AIN1 (default)
#define ADS1115_MUX_DIFF_0_3        (0x1000)    // Differential P = AIN0, N = AIN3
#define ADS1115_MUX_DIFF_1_3        (0x2000)    // Differential P = AIN1, N = AIN3
#define ADS1115_MUX_DIFF_2_3        (0x3000)    // Differential P = AIN2, N = AIN3
#define ADS1115_MUX_SINGLE_0        (0x4000)    // Single-ended AIN0
#define ADS1115_MUX_SINGLE_1        (0x5000)    // Single-ended AIN1
#define ADS1115_MUX_SINGLE_2        (0x6000)    // Single-ended AIN2
#define ADS1115_MUX_SINGLE_3        (0x7000)    // Single-ended AIN3
#define IS_CHANNEL_VALID(channel) (((channel) & 0x8FFF) == 0)

#define ADS1115_PGA_MASK            (0x0E00)
#define ADS1115_PGA_6_144V          (0x0000)    // +/-6.144V range = Gain 2/3
#define ADS1115_PGA_4_096V          (0x0200)    // +/-4.096V range = Gain 1
#define ADS1115_PGA_2_048V          (0x0400)    // +/-2.048V range = Gain 2 (default)
#define ADS1115_PGA_1_024V          (0x0600)    // +/-1.024V range = Gain 4
#define ADS1115_PGA_0_512V          (0x0800)    // +/-0.512V range = Gain 8
#define ADS1115_PGA_0_256V          (0x0A00)    // +/-0.256V range = Gain 16

#define ADS1115_MODE_MASK           (0x0100)
#define ADS1115_MODE_CONTIN         (0x0000)    // Continuous conversion mode
#define ADS1115_MODE_SINGLE         (0x0100)    // Power-down single-shot mode (default)

#define ADS1115_DR_MASK             (0x00E0)
#define ADS1115_DR_8SPS             (   8)
#define ADS1115_DR_16SPS            (  16)
#define ADS1115_DR_32SPS            (  32)
#define ADS1115_DR_64SPS            (  64)
#define ADS1115_DR_128SPS           ( 128)
#define ADS1115_DR_250SPS           ( 250)
#define ADS1115_DR_475SPS           ( 475)
#define ADS1115_DR_490SPS           ( 490)
#define ADS1115_DR_860SPS           ( 860)
#define ADS1115_DR_920SPS           ( 920)
#define ADS1115_DR_1600SPS          (1600)
#define ADS1115_DR_2400SPS          (2400)
#define ADS1115_DR_3300SPS          (3300)

#define ADS1115_CMODE_MASK          (0x0010)
#define ADS1115_CMODE_TRAD          (0x0000)    // Traditional comparator with hysteresis (default)
#define ADS1115_CMODE_WINDOW        (0x0010)    // Window comparator

#define ADS1115_CPOL_MASK           (0x0008)
#define ADS1115_CPOL_ACTVLOW        (0x0000)    // ALERT/RDY pin is low when active (default)
#define ADS1115_CPOL_ACTVHI         (0x0008)    // ALERT/RDY pin is high when active

#define ADS1115_CLAT_MASK           (0x0004)    // Determines if ALERT/RDY pin latches once asserted
#define ADS1115_CLAT_NONLAT         (0x0000)    // Non-latching comparator (default)
#define ADS1115_CLAT_LATCH          (0x0004)    // Latching comparator

#define ADS1115_CQUE_MASK           (0x0003)
#define ADS1115_CQUE_1CONV          (0x0000)    // Assert ALERT/RDY after one conversions
#define ADS1115_CQUE_2CONV          (0x0001)    // Assert ALERT/RDY after two conversions
#define ADS1115_CQUE_4CONV          (0x0002)    // Assert ALERT/RDY after four conversions
#define ADS1115_CQUE_NONE           (0x0003)    // Disable the comparator and put ALERT/RDY in high state (default)

#define ADS1115_DEFAULT_CONFIG_REG  (0x8583)    // Config register value after reset

// #define ADS1115_INCLUDE_TEST_FUNCTION

//***************************************************************************

static const uint8_t ads1115_i2c_id = 0;
static const uint8_t general_i2c_addr = 0x00;
static const uint8_t ads1115_i2c_reset = 0x06;
static const char metatable_name[] = "ads1115.device";
static const char unexpected_value[] = "unexpected value";

typedef struct {
    uint8_t i2c_addr;
    uint8_t chip_id;
    uint16_t gain;
    uint16_t samples_value; // sample per second
    uint16_t samples;       // register value
    uint16_t comp;
    uint16_t mode;
    uint16_t threshold_low;
    uint16_t threshold_hi;
    uint16_t config;
    int timer_ref;
    os_timer_t timer;
} ads_ctrl_ud_t;


static int ads1115_lua_readoutdone(void * param);
static int ads1115_lua_register(lua_State *L, uint8_t chip_id);

static uint8_t write_reg(uint8_t ads_addr, uint8_t reg, uint16_t config) {
    platform_i2c_send_start(ads1115_i2c_id);
    platform_i2c_send_address(ads1115_i2c_id, ads_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(ads1115_i2c_id, reg);
    platform_i2c_send_byte(ads1115_i2c_id, (uint8_t)(config >> 8));
    platform_i2c_send_byte(ads1115_i2c_id, (uint8_t)(config & 0xFF));
    platform_i2c_send_stop(ads1115_i2c_id);
}

static uint16_t read_reg(uint8_t ads_addr, uint8_t reg) {
    platform_i2c_send_start(ads1115_i2c_id);
    platform_i2c_send_address(ads1115_i2c_id, ads_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(ads1115_i2c_id, reg);
    platform_i2c_send_stop(ads1115_i2c_id);
    platform_i2c_send_start(ads1115_i2c_id);
    platform_i2c_send_address(ads1115_i2c_id, ads_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
    uint16_t buf = (platform_i2c_recv_byte(ads1115_i2c_id, 1) << 8);
    buf += platform_i2c_recv_byte(ads1115_i2c_id, 0);
    platform_i2c_send_stop(ads1115_i2c_id);
    return buf;
}

// convert ADC value to voltage corresponding to PGA settings
// returned voltage is in milivolts
static double get_mvolt(uint16_t gain, uint16_t value) {

    double volt = 0;

    switch (gain) {
        case (ADS1115_PGA_6_144V):
            volt = (int16_t)value * 0.1875;
            break;
        case (ADS1115_PGA_4_096V):
            volt = (int16_t)value * 0.125;
            break;
        case (ADS1115_PGA_2_048V):
            volt = (int16_t)value * 0.0625;
            break;
        case (ADS1115_PGA_1_024V):
            volt = (int16_t)value * 0.03125;
            break;
        case (ADS1115_PGA_0_512V):
            volt = (int16_t)value * 0.015625;
            break;
        case (ADS1115_PGA_0_256V):
            volt = (int16_t)value * 0.0078125;
            break;
    }

    return volt;
}

// validates and convert threshold in volt to ADC value corresponding to PGA settings
// returns true if valid
static uint8_t get_value(uint16_t gain, uint16_t channel, int16_t *volt) {
    switch (gain) {
        case (ADS1115_PGA_6_144V):
            if ((*volt >= 6144) || (*volt < -6144) || ((*volt < 0) && (channel >> 14))) return 0;
            *volt = *volt / 0.1875;
            break;
        case (ADS1115_PGA_4_096V):
            if ((*volt >= 4096) || (*volt < -4096) || ((*volt < 0) && (channel >> 14))) return 0;
            *volt = *volt / 0.125;
            break;
        case (ADS1115_PGA_2_048V):
            if ((*volt >= 2048) || (*volt < -2048) || ((*volt < 0) && (channel >> 14))) return 0;
            *volt = *volt / 0.0625;
            break;
        case (ADS1115_PGA_1_024V):
            if ((*volt >= 1024) || (*volt < -1024) || ((*volt < 0) && (channel >> 14))) return 0;
            *volt = *volt / 0.03125;
            break;
        case (ADS1115_PGA_0_512V):
            if ((*volt >= 512) || (*volt < -512) || ((*volt < 0) && (channel >> 14))) return 0;
            *volt = *volt / 0.015625;
            break;
        case (ADS1115_PGA_0_256V):
            if ((*volt >= 256) || (*volt < -256) || ((*volt < 0) && (channel >> 14))) return 0;
            *volt = *volt / 0.0078125;
            break;
    }
    return 1;
}


// Reset of all devices
// Lua:     ads1115.reset()
static int ads1115_lua_reset(lua_State *L) {
    platform_i2c_send_start(ads1115_i2c_id);
    platform_i2c_send_address(ads1115_i2c_id, general_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(ads1115_i2c_id, ads1115_i2c_reset);
    platform_i2c_send_stop(ads1115_i2c_id);
    return 0;
}

// Register an ADS device
// Lua:     ads1115.ADS1115(I2C_ID, ADDRESS)
static int ads1115_lua_register_1115(lua_State *L) {
    return ads1115_lua_register(L, ADS1115_ADS1115);
}

static int ads1115_lua_register_1015(lua_State *L) {
    return ads1115_lua_register(L, ADS1115_ADS1015);
}

static int ads1115_lua_register(lua_State *L, uint8_t chip_id) {
    uint8_t i2c_id = luaL_checkinteger(L, 1);
    luaL_argcheck(L, 0 == i2c_id, 1, "i2c_id must be 0");
    uint8_t i2c_addr = luaL_checkinteger(L, 2);
    luaL_argcheck(L, IS_I2C_ADDR_VALID(i2c_addr), 2, unexpected_value);
    uint16_t config_read = read_reg(i2c_addr, ADS1115_POINTER_CONFIG);
    if (config_read == 0xFFFF) {
        return luaL_error(L, "found no device");
    }
    if (config_read != ADS1115_DEFAULT_CONFIG_REG) {
        return luaL_error(L, "unexpected config value (%p) please reset device before calling this function", config_read);
    }
    ads_ctrl_ud_t *ads_ctrl = (ads_ctrl_ud_t *)lua_newuserdata(L, sizeof(ads_ctrl_ud_t));
    if (NULL == ads_ctrl) {
        return luaL_error(L, "ads1115 malloc: out of memory");
    }
    luaL_getmetatable(L, metatable_name);
    lua_setmetatable(L, -2);
    ads_ctrl->chip_id = chip_id;
    ads_ctrl->i2c_addr = i2c_addr;
    ads_ctrl->gain = ADS1115_PGA_6_144V;
    ads_ctrl->samples = ADS1115_DR_128SPS;
    ads_ctrl->samples_value = chip_id == ADS1115_ADS1115 ? 128 : 1600;
    ads_ctrl->comp = ADS1115_CQUE_NONE;
    ads_ctrl->mode = ADS1115_MODE_SINGLE;
    ads_ctrl->threshold_low = 0x8000;
    ads_ctrl->threshold_hi = 0x7FFF;
    ads_ctrl->config = ADS1115_DEFAULT_CONFIG_REG;
    ads_ctrl->timer_ref = LUA_NOREF;
    return 1;
}

// Change the ADC device settings
// Lua:     ads1115.device:settings(GAIN,SAMPLES,CHANNEL,MODE[,CONVERSION_RDY][,COMPARATOR,THRESHOLD_LOW,THRESHOLD_HI[,COMP_MODE])
static int ads1115_lua_setting(lua_State *L) {
    int argc = lua_gettop(L);
    if (argc != 5 && argc != 6 && argc != 8 && argc != 9) { // user data counts
        luaL_error(L, "invalid number of arguments to 'setting'");
    }
    ads_ctrl_ud_t *ads_ctrl = luaL_checkudata(L, 1, metatable_name);
    // gain
    uint16_t gain = luaL_checkinteger(L, 2);
    luaL_argcheck(L, (gain == ADS1115_PGA_6_144V) || (gain == ADS1115_PGA_4_096V) || (gain == ADS1115_PGA_2_048V) ||
                     (gain == ADS1115_PGA_1_024V) || (gain == ADS1115_PGA_0_512V) || (gain == ADS1115_PGA_0_256V),
                  2, unexpected_value);
    ads_ctrl->gain = gain;
    // samples
    uint16_t samples_value = luaL_checkinteger(L, 3);
    uint16_t samples = 0;
    if (ads_ctrl->chip_id == ADS1115_ADS1115) {
        switch(samples_value) {
            case ADS1115_DR_8SPS:
                samples = 0;
                break;
            case ADS1115_DR_16SPS:
                samples = 0x20;
                break;
            case ADS1115_DR_32SPS:
                samples = 0x40;
                break;
            case ADS1115_DR_64SPS:
                samples = 0x60;
                break;
            case ADS1115_DR_128SPS: // default
                samples = 0x80;
                break;
            case ADS1115_DR_250SPS:
                samples = 0xA0;
                break;
            case ADS1115_DR_475SPS:
                samples = 0xC0;
                break;
            case ADS1115_DR_860SPS:
                samples = 0xE0;
                break;
            default:
                luaL_argerror(L, 3, unexpected_value);
        }
    } else { // ADS1115_ADS1015
        switch(samples_value) {
            case ADS1115_DR_128SPS:
                samples = 0;
                break;
            case ADS1115_DR_250SPS:
                samples = 0x20;
                break;
            case ADS1115_DR_490SPS:
                samples = 0x40;
                break;
            case ADS1115_DR_920SPS:
                samples = 0x60;
                break;
            case ADS1115_DR_1600SPS: // default
                samples = 0x80;
                break;
            case ADS1115_DR_2400SPS:
                samples = 0xA0;
                break;
            case ADS1115_DR_3300SPS:
                samples = 0xC0;
                break;
            default:
                luaL_argerror(L, 3, unexpected_value);
        }
    }
    ads_ctrl->samples = samples;
    ads_ctrl->samples_value = samples_value;
    // channel
    uint16_t channel = luaL_checkinteger(L, 4);
    luaL_argcheck(L, IS_CHANNEL_VALID(channel), 4, unexpected_value);
    // mode
    uint16_t mode = luaL_checkinteger(L, 5);
    luaL_argcheck(L, (mode == ADS1115_MODE_SINGLE) || (mode == ADS1115_MODE_CONTIN), 5, unexpected_value);
    ads_ctrl->mode = mode;
    uint16_t os = mode == ADS1115_MODE_SINGLE ? ADS1115_OS_SINGLE : ADS1115_OS_NON;

    uint16_t comp = ADS1115_CQUE_NONE;
    // Parse optional parameters
    if (argc > 5) {
        // comparator or conversion count
        comp = luaL_checkinteger(L, 6);
        luaL_argcheck(L, (comp == ADS1115_CQUE_1CONV) || (comp == ADS1115_CQUE_2CONV) || (comp == ADS1115_CQUE_4CONV),
                      6, unexpected_value);
        uint16_t threshold_low = 0x7FFF;
        uint16_t threshold_hi = 0x8000;
        if (argc > 6) {
            // comparator thresholds
            threshold_low = luaL_checkinteger(L, 7);
            threshold_hi = luaL_checkinteger(L, 8);
            luaL_argcheck(L, (int16_t)threshold_low <= (int16_t)threshold_hi, 7, "threshold_low > threshold_hi");
            luaL_argcheck(L, get_value(gain, channel, &threshold_low), 7, unexpected_value);
            luaL_argcheck(L, get_value(gain, channel, &threshold_hi), 8, unexpected_value);
        }
        ads_ctrl->threshold_low = threshold_low;
        ads_ctrl->threshold_hi = threshold_hi;
        NODE_DBG("ads1115 low: %04x\n", threshold_low);
        NODE_DBG("ads1115 hi : %04x\n", threshold_hi);
        write_reg(ads_ctrl->i2c_addr, ADS1115_POINTER_THRESH_LOW, threshold_low);
        write_reg(ads_ctrl->i2c_addr, ADS1115_POINTER_THRESH_HI, threshold_hi);
    }
    ads_ctrl->comp = comp;

    uint16_t comparator_mode = ADS1115_CMODE_TRAD;
    if (argc == 9) {
        comparator_mode = luaL_checkinteger(L, 9);
        luaL_argcheck(L, (comparator_mode == ADS1115_CMODE_WINDOW) || (comparator_mode == ADS1115_CMODE_TRAD),
                      9, unexpected_value);
    }

    uint16_t config = (os | channel | gain | mode | samples | comparator_mode | ADS1115_CPOL_ACTVLOW | ADS1115_CLAT_NONLAT | comp);
    ads_ctrl->config = config;

    NODE_DBG("ads1115 config: %04x\n", ads_ctrl->config);
    write_reg(ads_ctrl->i2c_addr, ADS1115_POINTER_CONFIG, config);
    return 0;
}

// Read the conversion register from the ADC device
// Lua:     ads1115.device:startread(function(volt, voltdec, adc, sign) print(volt,voltdec,adc,sign) end)
static int ads1115_lua_startread(lua_State *L) {
    ads_ctrl_ud_t *ads_ctrl = luaL_checkudata(L, 1, metatable_name);

    if (((ads_ctrl->comp == ADS1115_CQUE_1CONV) ||
         (ads_ctrl->comp == ADS1115_CQUE_2CONV) ||
         (ads_ctrl->comp == ADS1115_CQUE_4CONV)) &&
        (ads_ctrl->threshold_low == 0x7FFF) &&
        (ads_ctrl->threshold_hi == 0x8000)) {
        // conversion ready mode
        if (ads_ctrl->mode == ADS1115_MODE_SINGLE) {
            NODE_DBG("ads1115 trigger config: %04x", ads_ctrl->config);
            write_reg(ads_ctrl->i2c_addr, ADS1115_POINTER_CONFIG, ads_ctrl->config);
        }
        return 0;
    }

    luaL_argcheck(L, (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION), 2, "Must be function");
    lua_pushvalue(L, 2);
    ads_ctrl->timer_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    if (ads_ctrl->mode == ADS1115_MODE_SINGLE) {
        write_reg(ads_ctrl->i2c_addr, ADS1115_POINTER_CONFIG, ads_ctrl->config);
    }

    // Start a timer to wait until ADC conversion is done
    os_timer_disarm(&ads_ctrl->timer);
    os_timer_setfn(&ads_ctrl->timer, (os_timer_func_t *)ads1115_lua_readoutdone, (void *)ads_ctrl);

    int msec = 1; // ADS1115_DR_1600SPS, ADS1115_DR_2400SPS, ADS1115_DR_3300SPS
    switch (ads_ctrl->samples_value) {
        case ADS1115_DR_8SPS:
            msec = 150;
            break;
        case ADS1115_DR_16SPS:
            msec = 75;
            break;
        case ADS1115_DR_32SPS:
            msec = 35;
            break;
        case ADS1115_DR_64SPS:
            msec = 20;
            break;
        case ADS1115_DR_128SPS:
            msec = 10;
            break;
        case ADS1115_DR_250SPS:
            msec = 5;
            break;
        case ADS1115_DR_475SPS:
        case ADS1115_DR_490SPS:
            msec = 3;
            break;
        case ADS1115_DR_860SPS:
        case ADS1115_DR_920SPS:
            msec = 2;
    }
    os_timer_arm(&ads_ctrl->timer, msec, 0);
    return 0;
}

static void read_common(ads_ctrl_ud_t * ads_ctrl, uint16_t raw, lua_State *L) {
    double mvolt = get_mvolt(ads_ctrl->gain, raw);
#ifdef LUA_NUMBER_INTEGRAL
    int sign;
    if (mvolt == 0) {
        sign = 0;
    } else if (mvolt > 0) {
        sign = 1;
    } else {
        sign = -1;
    }
    int uvolt;
    if (sign >= 0) {
        uvolt = (int)((mvolt - (int)mvolt) * 1000 + 0.5);
    } else {
        uvolt = -(int)((mvolt - (int)mvolt) * 1000 - 0.5);
        mvolt = -mvolt;
    }
    lua_pushnumber(L, mvolt);
    lua_pushinteger(L, uvolt);
    lua_pushinteger(L, raw);
    lua_pushinteger(L, sign);
#else
    lua_pushnumber(L, mvolt);
    lua_pushnil(L);
    lua_pushinteger(L, raw);
    lua_pushnil(L);
#endif
}


// adc conversion timer callback
static int ads1115_lua_readoutdone(void * param) {
    ads_ctrl_ud_t * ads_ctrl = (ads_ctrl_ud_t *)param;
    uint16_t raw = read_reg(ads_ctrl->i2c_addr, ADS1115_POINTER_CONVERSION);
    lua_State *L = lua_getstate();
    os_timer_disarm(&ads_ctrl->timer);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ads_ctrl->timer_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, ads_ctrl->timer_ref);
    ads_ctrl->timer_ref = LUA_NOREF;
    read_common(ads_ctrl, raw, L);
    lua_call(L, 4, 0);
}

// Read the conversion register from the ADC device
// Lua:     volt,voltdec,adc,sign = ads1115.device:read()
static int ads1115_lua_read(lua_State *L) {
    ads_ctrl_ud_t *ads_ctrl = luaL_checkudata(L, 1, metatable_name);
    uint16_t raw = read_reg(ads_ctrl->i2c_addr, ADS1115_POINTER_CONVERSION);
    read_common(ads_ctrl, raw, L);
    return 4;
}

#ifdef ADS1115_INCLUDE_TEST_FUNCTION
// this function simulates conversion using raw value provided as argument
// Lua:  volt,volt_dec,adc,sign = ads1115.test_volt_conversion(-1)
static int test_volt_conversion(lua_State *L) {
    ads_ctrl_ud_t *ads_ctrl = luaL_checkudata(L, 1, metatable_name);
    uint16_t raw = luaL_checkinteger(L, 2);
    read_common(ads_ctrl, raw, L);
    return 4;
}
#endif

static int ads1115_lua_delete(lua_State *L) {
    ads_ctrl_ud_t *ads_ctrl = luaL_checkudata(L, 1, metatable_name);
    if (ads_ctrl->timer_ref != LUA_NOREF) {
        os_timer_disarm(&ads_ctrl->timer);
        luaL_unref(L, LUA_REGISTRYINDEX, ads_ctrl->timer_ref);
    }
    return 0;
}

static const LUA_REG_TYPE ads1115_map[] = {

    {   LSTRKEY( "ads1115" ),       LFUNCVAL(ads1115_lua_register_1115) },
    {   LSTRKEY( "ads1015" ),       LFUNCVAL(ads1115_lua_register_1015) },
    {   LSTRKEY( "reset" ),         LFUNCVAL(ads1115_lua_reset)     },
    {   LSTRKEY( "ADDR_GND" ),      LNUMVAL(ADS1115_I2C_ADDR_GND)   },
    {   LSTRKEY( "ADDR_VDD" ),      LNUMVAL(ADS1115_I2C_ADDR_VDD)   },
    {   LSTRKEY( "ADDR_SDA" ),      LNUMVAL(ADS1115_I2C_ADDR_SDA)   },
    {   LSTRKEY( "ADDR_SCL" ),      LNUMVAL(ADS1115_I2C_ADDR_SCL)   },
    {   LSTRKEY( "SINGLE_SHOT" ),   LNUMVAL(ADS1115_MODE_SINGLE)    },
    {   LSTRKEY( "CONTINUOUS" ),    LNUMVAL(ADS1115_MODE_CONTIN)    },
    {   LSTRKEY( "DIFF_0_1" ),      LNUMVAL(ADS1115_MUX_DIFF_0_1)   },
    {   LSTRKEY( "DIFF_0_3" ),      LNUMVAL(ADS1115_MUX_DIFF_0_3)   },
    {   LSTRKEY( "DIFF_1_3" ),      LNUMVAL(ADS1115_MUX_DIFF_1_3)   },
    {   LSTRKEY( "DIFF_2_3" ),      LNUMVAL(ADS1115_MUX_DIFF_2_3)   },
    {   LSTRKEY( "SINGLE_0" ),      LNUMVAL(ADS1115_MUX_SINGLE_0)   },
    {   LSTRKEY( "SINGLE_1" ),      LNUMVAL(ADS1115_MUX_SINGLE_1)   },
    {   LSTRKEY( "SINGLE_2" ),      LNUMVAL(ADS1115_MUX_SINGLE_2)   },
    {   LSTRKEY( "SINGLE_3" ),      LNUMVAL(ADS1115_MUX_SINGLE_3)   },
    {   LSTRKEY( "GAIN_6_144V" ),   LNUMVAL(ADS1115_PGA_6_144V)     },
    {   LSTRKEY( "GAIN_4_096V" ),   LNUMVAL(ADS1115_PGA_4_096V)     },
    {   LSTRKEY( "GAIN_2_048V" ),   LNUMVAL(ADS1115_PGA_2_048V)     },
    {   LSTRKEY( "GAIN_1_024V" ),   LNUMVAL(ADS1115_PGA_1_024V)     },
    {   LSTRKEY( "GAIN_0_512V" ),   LNUMVAL(ADS1115_PGA_0_512V)     },
    {   LSTRKEY( "GAIN_0_256V" ),   LNUMVAL(ADS1115_PGA_0_256V)     },
    {   LSTRKEY( "DR_8SPS" ),       LNUMVAL(ADS1115_DR_8SPS)        },
    {   LSTRKEY( "DR_16SPS" ),      LNUMVAL(ADS1115_DR_16SPS)       },
    {   LSTRKEY( "DR_32SPS" ),      LNUMVAL(ADS1115_DR_32SPS)       },
    {   LSTRKEY( "DR_64SPS" ),      LNUMVAL(ADS1115_DR_64SPS)       },
    {   LSTRKEY( "DR_128SPS" ),     LNUMVAL(ADS1115_DR_128SPS)      },
    {   LSTRKEY( "DR_250SPS" ),     LNUMVAL(ADS1115_DR_250SPS)      },
    {   LSTRKEY( "DR_475SPS" ),     LNUMVAL(ADS1115_DR_475SPS)      },
    {   LSTRKEY( "DR_490SPS" ),     LNUMVAL(ADS1115_DR_490SPS)      },
    {   LSTRKEY( "DR_860SPS" ),     LNUMVAL(ADS1115_DR_860SPS)      },
    {   LSTRKEY( "DR_920SPS" ),     LNUMVAL(ADS1115_DR_920SPS)      },
    {   LSTRKEY( "DR_1600SPS" ),    LNUMVAL(ADS1115_DR_1600SPS)     },
    {   LSTRKEY( "DR_2400SPS" ),    LNUMVAL(ADS1115_DR_2400SPS)     },
    {   LSTRKEY( "DR_3300SPS" ),    LNUMVAL(ADS1115_DR_3300SPS)     },
    {   LSTRKEY( "CONV_RDY_1" ),    LNUMVAL(ADS1115_CQUE_1CONV)     },
    {   LSTRKEY( "CONV_RDY_2" ),    LNUMVAL(ADS1115_CQUE_2CONV)     },
    {   LSTRKEY( "CONV_RDY_4" ),    LNUMVAL(ADS1115_CQUE_4CONV)     },
    {   LSTRKEY( "COMP_1CONV" ),    LNUMVAL(ADS1115_CQUE_1CONV)     },
    {   LSTRKEY( "COMP_2CONV" ),    LNUMVAL(ADS1115_CQUE_2CONV)     },
    {   LSTRKEY( "COMP_4CONV" ),    LNUMVAL(ADS1115_CQUE_4CONV)     },
    {   LSTRKEY( "CMODE_TRAD"),     LNUMVAL(ADS1115_CMODE_TRAD)     },
    {   LSTRKEY( "CMODE_WINDOW"),   LNUMVAL(ADS1115_CMODE_WINDOW)   },
    {   LNILKEY, LNILVAL                                            }
};

static const LUA_REG_TYPE ads1115_instance_map[] = {
    {   LSTRKEY( "setting" ),       LFUNCVAL(ads1115_lua_setting)   },
    {   LSTRKEY( "startread" ),     LFUNCVAL(ads1115_lua_startread) },
    {   LSTRKEY( "read" ),          LFUNCVAL(ads1115_lua_read)      },
#ifdef ADS1115_INCLUDE_TEST_FUNCTION
    {   LSTRKEY( "test_volt_conversion" ), LFUNCVAL(test_volt_conversion)},
#endif
    {   LSTRKEY( "__index" ),       LROVAL(ads1115_instance_map)    },
    {   LSTRKEY( "__gc" ),          LFUNCVAL(ads1115_lua_delete)    },
    {   LNILKEY, LNILVAL                                            }
};


int luaopen_ads1115(lua_State *L) {
    luaL_rometatable(L, metatable_name, (void *)ads1115_instance_map);
    return 0;
}

NODEMCU_MODULE(ADS1115, "ads1115", ads1115_map, luaopen_ads1115);
