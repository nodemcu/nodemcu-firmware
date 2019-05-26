/*
 * Driver for mpu6050
 *
 * Code based on hmc5883l driver
 * author: feilongphone@gmail.com
 */
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"

static const uint32_t mpu6050_i2c_id = 0;
static const uint8_t mpu6050_i2c_addr = 0x68;
static const uint8_t mpu6050_i2c_whoami = 0x68;

static uint8_t r8u(uint32_t id, uint8_t reg) {
    uint8_t ret;

    platform_i2c_send_start(id);
    platform_i2c_send_address(id, mpu6050_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(id, reg);
    platform_i2c_send_stop(id);
    platform_i2c_send_start(id);
    platform_i2c_send_address(id, mpu6050_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
    ret = platform_i2c_recv_byte(id, 0);
    platform_i2c_send_stop(id);
    return ret;
}

static void w8u(uint32_t id, uint8_t reg, uint8_t val) {
    platform_i2c_send_start(mpu6050_i2c_id);
    platform_i2c_send_address(mpu6050_i2c_id, mpu6050_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(mpu6050_i2c_id, reg);
    platform_i2c_send_byte(mpu6050_i2c_id, val);
    platform_i2c_send_stop(mpu6050_i2c_id);
}

static int mpu6050_setup(lua_State* L) {
    uint8_t  wmi;

    wmi = r8u(mpu6050_i2c_id, 0x75);

    if (wmi != mpu6050_i2c_whoami) {
        return luaL_error(L, "device id dismatch");
    }

    // power on
    w8u(mpu6050_i2c_id, 0x6B, 0x00);

    return 0;
}

static int mpu6050_read(lua_State* L) {
#define MpuDataLength 14
    uint8_t data[MpuDataLength];
    int ax,ay,az,temp,gx,gy,gz;
    int i;

    platform_i2c_send_start(mpu6050_i2c_id);
    platform_i2c_send_address(mpu6050_i2c_id, mpu6050_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(mpu6050_i2c_id, 0x3B);
    platform_i2c_send_stop(mpu6050_i2c_id);
    platform_i2c_send_start(mpu6050_i2c_id);
    platform_i2c_send_address(mpu6050_i2c_id, mpu6050_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);

    for (i=0; i<MpuDataLength-1; i++) {
        data[i] = platform_i2c_recv_byte(mpu6050_i2c_id, 1);
    }

    data[MpuDataLength-1] = platform_i2c_recv_byte(mpu6050_i2c_id, 0);

    platform_i2c_send_stop(mpu6050_i2c_id);

    ax = (int16_t) ((data[0] << 8) | data[1]);
    ay = (int16_t) ((data[2] << 8) | data[3]);
    az = (int16_t) ((data[4] << 8) | data[5]);
    temp = (int16_t) ((data[6] << 8) | data[7]);
    gx = (int16_t) ((data[8] << 8) | data[9]);
    gy = (int16_t) ((data[10] << 8) | data[11]);
    gz = (int16_t) ((data[12] << 8) | data[13]);

    lua_pushinteger(L, ax);
    lua_pushinteger(L, ay);
    lua_pushinteger(L, az);
    lua_pushinteger(L, temp);
    lua_pushinteger(L, gx);
    lua_pushinteger(L, gy);
    lua_pushinteger(L, gz);

    return 7;
}

LROT_BEGIN(mpu6050)
  LROT_FUNCENTRY( read, mpu6050_read )
  LROT_FUNCENTRY( setup, mpu6050_setup )
LROT_END( mpu6050, NULL, 0 )

NODEMCU_MODULE(MPU6050, "mpu6050", mpu6050, NULL);
