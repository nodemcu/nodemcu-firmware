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

#define CONFIG 0x1A
#define GYRO_CONFIG 0x1B
#define ACCEL_CONFIG 0x1C

typedef struct
{
    int16_t X;
    int16_t Y;
    int16_t Z;
} TrisAxisData_t;

typedef struct
{
    TrisAxisData_t Accel;
    int16_t Temp;
    TrisAxisData_t Gyro;
} MPU6050DATA_t;

typedef struct
{
    uint8_t config;
    uint8_t Accel;
    uint8_t gyro;
} Mpu6050Config_t;

MPU6050DATA_t MpuOffset;

#define I2CREADSTART(i2cid, addr)                                                           \
    platform_i2c_send_start(i2cid);                                                         \
    platform_i2c_send_address(i2cid, mpu6050_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER); \
    platform_i2c_send_byte(i2cid, addr);                                                    \
    platform_i2c_send_stop(i2cid);                                                          \
    platform_i2c_send_start(i2cid);                                                         \
    platform_i2c_send_address(i2cid, mpu6050_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER)

#define I2C_ACK(index, count) ((index) == (count)-1) ? 0 : 1
#define GetBaseNumber(num, div) ((num) / (div) * (div))
#define AddressTrans(type, addr) ((sizeof(type) == 1) ? (addr) : GetBaseNumber((addr), sizeof(type)) + sizeof(type) - 1 - (addr) % sizeof(type))

#define TYPESIZE(ta, tb) sizeof(ta) / sizeof(tb)

#define STRUCTFOREACH(ta, tb, func)                  \
    for (int __i = 0; __i < TYPESIZE(ta, tb); __i++) \
    {                                                \
        func;                                        \
    }

#define STRUCTFOREACHSET(ta, tb, point, func) \
    STRUCTFOREACH(ta, tb, *((tb *)point + __i) = func)

#define STRUCTFOREACHSETTRANS(ta, tb, tori, point, func) \
    STRUCTFOREACH(ta, tb, *((tb *)point + AddressTrans(tori, __i)) = func)

#define Mpu6050SetConfig(id, addr, config, default) \
    if (config != default)                          \
    w8u(id, addr, config)

static uint8_t r8u(uint32_t id, uint8_t reg)
{
    uint8_t ret;

    I2CREADSTART(mpu6050_i2c_id, reg);
    ret = platform_i2c_recv_byte(id, 0);
    platform_i2c_send_stop(id);
    return ret;
}

static void w8u(uint32_t id, uint8_t reg, uint8_t val)
{
    platform_i2c_send_start(mpu6050_i2c_id);
    platform_i2c_send_address(mpu6050_i2c_id, mpu6050_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(mpu6050_i2c_id, reg);
    platform_i2c_send_byte(mpu6050_i2c_id, val);
    platform_i2c_send_stop(mpu6050_i2c_id);
}

static int mpu6050_setup(Mpu6050Config_t *config)
{
    // power on
    w8u(mpu6050_i2c_id, 0x6B, 0x00);

    // config
    uint8_t *configPoint = (uint8_t *)config;
    // for (int i = 0; i < TYPESIZE(Mpu6050Config_t, uint8_t); i++)
    // {
    //     Mpu6050SetConfig(mpu6050_i2c_id, 0x6B, *(configPoint + i), 0x00);
    // }
    STRUCTFOREACH(Mpu6050Config_t, uint8_t, Mpu6050SetConfig(mpu6050_i2c_id, 0x6B, *(configPoint + __i), 0x00));

    return 0;
}

static uint8_t mpu6050_whoani()
{
    return r8u(mpu6050_i2c_id, 0x75);
}

static MPU6050DATA_t *mpu6050_read()
{
    static MPU6050DATA_t data;
    int i;

    I2CREADSTART(mpu6050_i2c_id, 0x3B);
    MPU6050DATA_t *dataPoint = &data;
    // for (i = 0; i < TYPESIZE(MPU6050DATA_t, uint8_t); i++)
    // {
    //     *((uint8_t *)dataPoint + AddressTrans(int16_t, i)) =
    //         platform_i2c_recv_byte(mpu6050_i2c_id, I2C_ACK(i, TYPESIZE(MPU6050DATA_t, uint8_t) - 1));
    // }
    STRUCTFOREACHSETTRANS(MPU6050DATA_t, uint8_t, int16_t, dataPoint,
                          platform_i2c_recv_byte(mpu6050_i2c_id, I2C_ACK(__i, TYPESIZE(MPU6050DATA_t, uint8_t))))

    platform_i2c_send_stop(mpu6050_i2c_id);

    return dataPoint;
}

#define AVERAGE2(a, b) (((a) + (b)) / 2)
static void MpuCalcOffset(int times)
{
    MPU6050DATA_t *offset = &MpuOffset;
    c_memcpy(&offset->Gyro, &(mpu6050_read())->Gyro, sizeof(TrisAxisData_t));
    for (int i = 0; i < times; i++)
    {
        MPU6050DATA_t *d = mpu6050_read();
        TrisAxisData_t *gyro = (TrisAxisData_t *)(&d->Gyro);
        // for (int j = 0; j < TYPESIZE(TrisAxisData_t, int16_t); j++)
        // {
        //     int16_t *dp = (int16_t *)gyro + j;   // gyro data point
        //     int16_t *op = (int16_t *)offset + j; // offset point
        //     *op = (*dp + *op) / 2;
        // }
        gyro->X = AVERAGE2(gyro->X, d->Gyro.X);
        gyro->Y = AVERAGE2(gyro->Y, d->Gyro.Y);
        gyro->Z = AVERAGE2(gyro->Z, d->Gyro.Z);
        system_soft_wdt_feed(); // feed the dog
    }
}

static MPU6050DATA_t *mpu6050_read_offset()
{
    int16_t *data = (int16_t *)mpu6050_read();
    int16_t *offset = (int16_t *)&MpuOffset;
    static MPU6050DATA_t result;
    int16_t *resultPoint = (int16_t *)&result;

    STRUCTFOREACHSET(MPU6050DATA_t, int16_t, resultPoint, *(data + __i) - *(offset + __i))

    return (MPU6050DATA_t *)resultPoint;
}

static int lua_read_ori(lua_State *L)
{
    const int16_t *dataPoint = (const int16_t *)mpu6050_read();

    // for (int i = 0; i < TYPESIZE(MPU6050DATA_t, int16_t); i++)
    // {
    //     lua_pushinteger(L, *((int16_t *)dataPoint + i));
    // }
    // STRUCTFOREACH(MPU6050DATA_t, int16_t, lua_pushinteger(L, *((int16_t *)dataPoint + i)))
    STRUCTFOREACH(MPU6050DATA_t, int16_t, lua_pushinteger(L, *(dataPoint + __i)))

    return TYPESIZE(MPU6050DATA_t, int16_t);
}

// return normalize number -1~1
static int lua_read(lua_State *L)
{
    const int16_t *dataPoint = (const int16_t *)mpu6050_read_offset();

    // for (int i = 0; i < TYPESIZE(MPU6050DATA_t, int16_t); i++)
    // {
    //     lua_pushinteger(L, *((int16_t *)dataPoint + i));
    // }
    STRUCTFOREACH(MPU6050DATA_t, int16_t,
                  lua_pushnumber(L, (double)*(dataPoint + __i) / (0x10000 / 2 - 1)))

    return TYPESIZE(MPU6050DATA_t, int16_t);
}

static int lua_setup(lua_State *L)
{
    uint8_t wmi;

    wmi = mpu6050_whoani();

    if (wmi != mpu6050_i2c_whoami)
    {
        return luaL_error(L, "device id %x dismatch", wmi);
    }

    // read config
    Mpu6050Config_t config;
    Mpu6050Config_t *configPoint = &config;

    STRUCTFOREACHSET(Mpu6050Config_t, uint8_t, configPoint, luaL_checkinteger(L, __i + 1))

    mpu6050_setup(configPoint); // config

    STRUCTFOREACHSET(MPU6050DATA_t, uint8_t, (uint8_t *)&MpuOffset, 0) // set offset to 0
}

static int lua_get_offset(lua_State *L)
{
    const int16_t *dataPoint = (const int16_t *)&MpuOffset;

    STRUCTFOREACH(MPU6050DATA_t, int16_t,
                  lua_pushnumber(L, *(dataPoint + __i)))

    return TYPESIZE(MPU6050DATA_t, int16_t);
}

static int lua_calc_offset(lua_State *L)
{
    const int calcTimes = luaL_checkinteger(L, 1);

    MpuCalcOffset(calcTimes);

    return lua_get_offset(L);
}

static int lua_set_offset(lua_State *L)
{
    const int16_t *dataPoint = (const int16_t *)&MpuOffset;

    STRUCTFOREACHSET(MPU6050DATA_t, uint8_t, dataPoint, luaL_checkinteger(L, __i + 1))

    return lua_get_offset(L);
}

LROT_BEGIN(mpu6050)
LROT_FUNCENTRY(read, lua_read)
LROT_FUNCENTRY(readraw, lua_read_ori)
LROT_FUNCENTRY(calcOffset, lua_calc_offset)
LROT_FUNCENTRY(getOffset, lua_get_offset)
LROT_FUNCENTRY(setOffset, lua_set_offset)
LROT_FUNCENTRY(setup, lua_setup)
LROT_END(mpu6050, NULL, 0)

NODEMCU_MODULE(MPU6050, "mpu6050", mpu6050, NULL);
