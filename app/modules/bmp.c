#include "c_stdlib.h"
#include "lualib.h"
#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "user_interface.h"

#include "espconn.h"
#include "driver/i2c_master.h"

#include <stdlib.h>

#include "c_types.h"
#include "c_string.h"

#include "c_math.h"
#include "lauxlib.h"
#include "auxmods.h"
#include "lrotable.h"
#include "platform.h"

#include "driver/bmp280.h"


static int BMP280_init(u8_t sda, u8_t scl)
{

    u8_t id;

    platform_i2c_setup(BMP280_I2C_BUS_ID, sda, scl, PLATFORM_I2C_SPEED_SLOW);

    if(BMP280_readBytes(BMP280_CHIPID_ADDR, &id, 1)) {
        return 1;
    }

    if (id != BMP280_CHIPID_VAL) {
        return 1;
    }

    return 0;
}

static int BMP280_reset()
{
    u8_t reset = BMP280_RESET_VAL;
    if(BMP280_writeBytes(BMP280_RESET_ADDR, &reset, 1)) {
        return 1;
    }
    return 0;
}

static int BMP280_config()
{
    u8_t config = BMP280_CTRL_MEAS_VAL;
    if (BMP280_writeBytes(BMP280_CTRL_MEAS_ADDR, &config, 1)) {
        return 1;
    }
    return 0;
}

static int BMP280_readBytes(u8_t reg_addr, u8_t *dst, u8_t length)
{
    u8_t x;

    platform_i2c_send_start  ( BMP280_I2C_BUS_ID );
    platform_i2c_send_address( BMP280_I2C_BUS_ID, BMP280_ADDR, PLATFORM_I2C_DIRECTION_TRANSMITTER);

    if( platform_i2c_send_byte( BMP280_I2C_BUS_ID, reg_addr ) == 0 ) {
        return 1;
    }

    platform_i2c_send_start  ( BMP280_I2C_BUS_ID );
    platform_i2c_send_address( BMP280_I2C_BUS_ID , BMP280_ADDR, PLATFORM_I2C_DIRECTION_RECEIVER);

    for( x = 0; x < length; x ++ ) {
        dst[x] = platform_i2c_recv_byte( BMP280_I2C_BUS_ID, x < length - 1 );
    }
    platform_i2c_send_stop( BMP280_I2C_BUS_ID );
    return 0;

}


static int BMP280_writeBytes(u8_t reg_addr, u8_t *src, u8_t length)
{
    u8_t x;

    platform_i2c_send_start  ( BMP280_I2C_BUS_ID );
    platform_i2c_send_address( BMP280_I2C_BUS_ID, BMP280_ADDR, PLATFORM_I2C_DIRECTION_TRANSMITTER);

    if (platform_i2c_send_byte(BMP280_I2C_BUS_ID , reg_addr) == 0) {
        return 1;
    }

    for(x = 0; x < length; x ++ ) {
        if (platform_i2c_send_byte(BMP280_I2C_BUS_ID , src[x]) == 0) {
            return 1;
        }
    }

    platform_i2c_send_stop(BMP280_I2C_BUS_ID);
    return 0;
}


u16_t bmp_t1=0, bmp_p1=0;
s16_t bmp_t2, bmp_t3, bmp_p2, bmp_p3, bmp_p4, bmp_p5, bmp_p6, bmp_p7, bmp_p8, bmp_p9;

#define BMP_UNPACK(x) (((u16_t)(data[x+1]))<<8 | ((u16_t)(data[x])))


static int BMP280_get_calibration_data() {

    //from BMP280 datasheet

    //0x88 / 0x89    bmp_T1  ushort
    //0x8A / 0x8B    bmp_T2   short
    //0x8C / 0x8D    bmp_T3   short
    //0x8E / 0x8F    bmp_P1  ushort
    //0x90 / 0x91    bmp_P2   short
    //0x92 / 0x93    bmp_P3   short
    //0x94 / 0x95    bmp_P4   short
    //0x96 / 0x97    bmp_P5   short
    //0x98 / 0x99    bmp_P6   short
    //0x9A / 0x9B    bmp_P7   short
    //0x9C / 0x9D    bmp_P8   short
    //0x9E / 0x9F    bmp_P9   short

    u8_t data[24];

    if(BMP280_readBytes(BMP280_CALIB_START,data,24)) {
        return 1;
    }

    bmp_t1 = (u16_t)BMP_UNPACK(0);
    bmp_t2 = (s16_t)BMP_UNPACK(2);
    bmp_t3 = (s16_t)BMP_UNPACK(4);
    bmp_p1 = (u16_t)BMP_UNPACK(6);
    bmp_p2 = (s16_t)BMP_UNPACK(8);
    bmp_p3 = (s16_t)BMP_UNPACK(10);
    bmp_p4 = (s16_t)BMP_UNPACK(12);
    bmp_p5 = (s16_t)BMP_UNPACK(14);
    bmp_p6 = (s16_t)BMP_UNPACK(16);
    bmp_p7 = (s16_t)BMP_UNPACK(18);
    bmp_p8 = (s16_t)BMP_UNPACK(20);
    bmp_p9 = (s16_t)BMP_UNPACK(22);

    return 0;

}


s32_t bmp_uP, bmp_uT;

static int BMP280_get_sample() {

    u8_t data[6];

    if(BMP280_readBytes(BMP280_DATA_START,data,6)) {
        return 1;
    }

    bmp_uP = (s32_t)(
                    (((u32_t)(data[0])) << 12) |
                    (((u32_t)(data[1])) << 4 ) |
                    (((u32_t)(data[2])) >> 4 )
    );


    bmp_uT = (s32_t)(
                    (((u32_t)(data[3])) << 12) |
                    (((u32_t)(data[4])) << 4 ) |
                    (((u32_t)(data[5])) >> 4 )
    );

    return 0;
}

#define BMP280_S32_t s32_t
s32_t bmp_T, bmp_t_fine;
u32_t bmp_P;

static int BMP280_calcT() {
    s32_t var1, var2;
    var1 = ((((bmp_uT>>3) - ((BMP280_S32_t)bmp_t1<<1))) * ((BMP280_S32_t)bmp_t2)) >> 11;
    var2 = (((((bmp_uT>>4) - ((BMP280_S32_t)bmp_t1)) * ((bmp_uT>>4) - ((BMP280_S32_t)bmp_t1))) >> 12) *
            ((BMP280_S32_t)bmp_t3)) >> 14;
    bmp_t_fine = var1 + var2;
    bmp_T =(bmp_t_fine*5+128)>>8;
    return 0;
}


static int BMP280_calcP() {
        s32_t v_x1_u32r,
                v_x2_u32r;

        v_x1_u32r = (((s32_t)bmp_t_fine) >> 1) - (s32_t)64000;

        v_x2_u32r = (((v_x1_u32r >> 2) * (v_x1_u32r >> 2)) >> 11) * (bmp_p6);
        v_x2_u32r = v_x2_u32r + ((v_x1_u32r *(bmp_p5)) << 1);
        v_x2_u32r = (v_x2_u32r >> 2) + ((bmp_p4) << 16);
        v_x1_u32r = (((bmp_p3 * (((v_x1_u32r >> 2) * (v_x1_u32r >> 2)) >> 13)) >> 3) + (((bmp_p2) * v_x1_u32r) >> 1)) >> 18;
        v_x1_u32r = ((((32768+v_x1_u32r)) * (bmp_p1))	>> 15);
        bmp_P = (((u32_t)(((s32_t)1048576) - bmp_uP) -(v_x2_u32r >> 12))) * 3125;
        if (bmp_P < 0x80000000)
            /* Avoid exception caused by division by zero */
            if (v_x1_u32r != 0)
                bmp_P = (bmp_P << 1) / ((u32)v_x1_u32r);
            else
                return 1;
        else
            if (v_x1_u32r != 0)
                bmp_P = (bmp_P / (u32)v_x1_u32r) * 2;
            else
                return 1;

        v_x1_u32r = (((s32_t)bmp_p9) * ((s32_t)(((bmp_P >> 3) * (bmp_P >> 3)) >> 13))) >> 12;
        v_x2_u32r = (((s32_t)(bmp_P >> 2)) * ((s32_t)bmp_p8)) >> 13;
        bmp_P = (u32_t) ((s32_t)bmp_P + ((v_x1_u32r + v_x2_u32r + bmp_p7) >> 4));

        return 0;
}


#define LED_PIN 7

//direction = 0 down
//direction = 1 up
static int _bmp_glow(u8_t direction, u16_t duration_ms) {

    //max duty is 1023

    double start = duration_ms;

    platform_pwm_setup( LED_PIN, 1000, 0);
    platform_pwm_start( LED_PIN );


    while (duration_ms > 0) {

        if(direction == 0) {
            platform_pwm_set_duty( LED_PIN, duration_ms/start * 1000.0);
        } else {
            platform_pwm_set_duty( LED_PIN, (start-duration_ms)/start*1000.0);
        }

        os_delay_us( 1000 );
        duration_ms -= 1;
    }

    platform_pwm_stop(LED_PIN);
    return 0;
}

static int bmp_glow(lua_State *L) {
    u8_t direction  = luaL_checkinteger(L, 1);
    u16_t duration = luaL_checkinteger(L,2);
    _bmp_glow(direction,duration);
    return 0;
}

static int bmp_blink(lua_State *L) {

    u8_t count = luaL_checkinteger(L, 1);
    //3 times a second

    while (count > 0){
        _bmp_glow(1,100);
        _bmp_glow(0,100);
        os_delay_us(1000*100);
        count -= 1;
    }

    return 0;
}

// Lua: bmp.init( sda, scl )  eg bmp.init(4, 5)
static int bmp_init( lua_State *L ) {

    u8_t sda = luaL_checkinteger( L, 1 ),
         scl = luaL_checkinteger( L, 2 );

    if (BMP280_init(sda,scl)){
        return luaL_error( L, "Unable to communicate with BMP280" );
    }

    if (BMP280_get_calibration_data()) {
        return luaL_error(L, "Unable to get calibration data from BMP280");
    }

    if (BMP280_reset()) {
        return luaL_error(L, "Unable to reset BMP280");
    }

    return 0;
}

static int bmp_status(lua_State *L) {
    u8_t status[3];

    if (BMP280_readBytes(BMP280_CONFIG_START, status, 3)){
        return luaL_error(L, "Unable to read config values from BMP280");
    }

    lua_pushinteger(L,status[0]);
    lua_pushinteger(L,status[1]);
    lua_pushinteger(L,status[2]);

    return 3;

}

static int bmp_config(lua_State *L) {
//    if (BMP280_get_calibration_data()) {
//        return luaL_error(L, "Unable to read config values from BMP280");
//    }
    lua_pushinteger(L,bmp_t1);
    lua_pushinteger(L,bmp_t2);
    lua_pushinteger(L,bmp_t3);
    lua_pushinteger(L,bmp_p1);
    lua_pushinteger(L,bmp_p2);
    lua_pushinteger(L,bmp_p3);
    lua_pushinteger(L,bmp_p4);
    lua_pushinteger(L,bmp_p5);
    lua_pushinteger(L,bmp_p6);
    lua_pushinteger(L,bmp_p7);
    lua_pushinteger(L,bmp_p8);
    lua_pushinteger(L,bmp_p9);

    return 12;
}

static int bmp_sample(lua_State *L) {
//    if (bmp_t1 ==0 && bmp_p1 == 0) {
//        return luaL_error(L, "Read config data first.. bmp.config()");
//    }
    if (BMP280_config()){
        return luaL_error(L, "Unable to write config data to BMP280 ");
    }

    os_delay_us( BMP280_FORCED_SAMPLE_DELAY );

    if (BMP280_get_sample()) {
        return luaL_error(L, "Unable to read sample from BMP280");
    }

    BMP280_calcT();
    lua_pushnumber(L,bmp_T);
    if(BMP280_calcP()){
        lua_pushnumber(L,0);
    } else {
        lua_pushnumber(L,bmp_P);
    }
    return 2;
}



// Module function map
#define MIN_OPT_LEVEL   2
#include "lrodefs.h"

const LUA_REG_TYPE bmp_map[] =
{
        { LSTRKEY( "init" ),  LFUNCVAL(bmp_init) },
        { LSTRKEY( "status" ),  LFUNCVAL(bmp_status) },
        { LSTRKEY( "config" ),  LFUNCVAL(bmp_config) },
        { LSTRKEY( "sample" ),  LFUNCVAL(bmp_sample) },
        { LSTRKEY( "blink" ),  LFUNCVAL(bmp_blink) },
        { LSTRKEY( "glow" ),  LFUNCVAL(bmp_glow) },

#if LUA_OPTIMIZE_MEMORY > 0
#endif
    { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_bmp( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
    luaL_register( L, AUXLIB_BMP, bmp_map );
    
    // Add the constants
    
    return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}


