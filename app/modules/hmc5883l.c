/*
 * Driver for Honeywell HMC5883L compass IC
 *
 * Code based on BMP085 driver.
 */
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"

#define QMC5883L_ADDR 0x0D
#define HMC5883L_ADDR 0x1E

static const uint32_t i2c_id = 0;
static uint8_t i2c_addr;

static uint8_t r8u(uint32_t id, uint8_t i2c_addr, uint8_t reg) {
    uint8_t ret;

    platform_i2c_send_start(id);
    platform_i2c_send_address(id, i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(id, reg);
    platform_i2c_send_stop(id);
    platform_i2c_send_start(id);
    platform_i2c_send_address(id, i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
    ret = platform_i2c_recv_byte(id, 0);
    platform_i2c_send_stop(id);
    return ret;
}

static void w8u(uint32_t id, uint8_t i2c_addr, uint8_t reg, uint8_t val) {
    platform_i2c_send_start(id);
    platform_i2c_send_address(id, i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(id, reg);
    platform_i2c_send_byte(id, val);
    platform_i2c_send_stop(id);
}

static uint8_t xmc5883_identify() {
    uint8_t  devid_a, devid_b, devid_c;

    devid_a = r8u(i2c_id, HMC5883L_ADDR, 10);
    devid_b = r8u(i2c_id, HMC5883L_ADDR, 11);
    devid_c = r8u(i2c_id, HMC5883L_ADDR, 12);

    if ((devid_a == 0x48) && (devid_b == 0x34) && (devid_c == 0x33)) {
        return HMC5883L_ADDR;
    }

    uint8_t  devid;

    devid = r8u(i2c_id, QMC5883L_ADDR, 0x0D);
    if (devid == 0xFF) {
        return QMC5883L_ADDR;
    }
    return 0xFF; // means device not recognized
}

static int qmc5883_setup(lua_State* L) {
    /**
     * Define
     * OSR = 512
     * Full Scale Range = 8G(Gauss)
     * ODR = 200HZ
     * set continuous measurement mode
     */
    w8u(i2c_id, QMC5883L_ADDR, 0x0B, 0x01);
    w8u(i2c_id, QMC5883L_ADDR, 0x09, 0x1D);

    return 0;
}

static int hmc5883_setup(lua_State* L) {
    // 8 sample average, 15 Hz update rate, normal measurement
    w8u(i2c_id, HMC5883L_ADDR, 0x00, 0x70);

    // Gain = 5
    w8u(i2c_id, HMC5883L_ADDR, 0x01, 0xA0);

    // Continuous measurement
    w8u(i2c_id, HMC5883L_ADDR, 0x02, 0x00);

    return 0;
}

static int xmc5883_setup(lua_State* L) {
    i2c_addr = xmc5883_identify();
    if (i2c_addr == HMC5883L_ADDR) {
        return hmc5883_setup(L);
    } else if (i2c_addr == QMC5883L_ADDR) {
        return qmc5883_setup(L);
    }
    return luaL_error(L, "device not found");
}


static int xmc5883_init(lua_State* L) {
    uint32_t sda;
    uint32_t scl;

    platform_print_deprecation_note("hmc5883l.init() is replaced by hmc5883l.setup()", "in the next version");

    sda = luaL_checkinteger(L, 1);
    scl = luaL_checkinteger(L, 2);

    luaL_argcheck(L, sda > 0 && scl > 0, 1, "no i2c for D0");

    platform_i2c_setup(i2c_id, sda, scl, PLATFORM_I2C_SPEED_SLOW);

    return xmc5883_setup(L);
}

static int qmc5883_read(lua_State* L) {

    uint8_t data[6];
    int x,y,z;
    int i;

    platform_i2c_send_start(i2c_id);
    platform_i2c_send_address(i2c_id, QMC5883L_ADDR, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(i2c_id, 0x00);
    platform_i2c_send_stop(i2c_id);
    platform_i2c_send_start(i2c_id);
    platform_i2c_send_address(i2c_id, QMC5883L_ADDR, PLATFORM_I2C_DIRECTION_RECEIVER);

    for (i=0; i<5; i++) {
	    data[i] = platform_i2c_recv_byte(i2c_id, 1);
    }
    
    data[5] = platform_i2c_recv_byte(i2c_id, 0);

    platform_i2c_send_stop(i2c_id);

    x = (int16_t) (data[0] | (data[1] << 8));
    y = (int16_t) (data[2] | (data[3] << 8));
    z = (int16_t) (data[4] | (data[5] << 8));

    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    lua_pushinteger(L, z);

    return 3;
}

static int hmc5883_read(lua_State* L) {

    uint8_t data[6];
    int x,y,z;
    int i;

    platform_i2c_send_start(i2c_id);
    platform_i2c_send_address(i2c_id, HMC5883L_ADDR, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(i2c_id, 0x03);
    platform_i2c_send_stop(i2c_id);
    platform_i2c_send_start(i2c_id);
    platform_i2c_send_address(i2c_id, HMC5883L_ADDR, PLATFORM_I2C_DIRECTION_RECEIVER);

    for (i=0; i<5; i++) {
	      data[i] = platform_i2c_recv_byte(i2c_id, 1);
    }
    
    data[5] = platform_i2c_recv_byte(i2c_id, 0);

    platform_i2c_send_stop(i2c_id);

    x = (int16_t) ((data[0] << 8) | data[1]);
    z = (int16_t) ((data[2] << 8) | data[3]);
    y = (int16_t) ((data[4] << 8) | data[5]);

    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    lua_pushinteger(L, z);

    return 3;
}

static int xmc5883_read(lua_State* L) {
    if (i2c_addr == HMC5883L_ADDR) {
        return hmc5883_read(L);
    } else if (i2c_addr == QMC5883L_ADDR) {
        return qmc5883_read(L);
    }
    return luaL_error(L, "device not found");
}

static const LUA_REG_TYPE hmc5883_map[] = {
    { LSTRKEY( "read" ),         LFUNCVAL( xmc5883_read )},
    { LSTRKEY( "setup" ),        LFUNCVAL( xmc5883_setup )},
    // init() is deprecated
    { LSTRKEY( "init" ),         LFUNCVAL( xmc5883_init )},
    { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(HMC5883L, "hmc5883l", hmc5883_map, NULL);
