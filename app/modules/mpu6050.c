/*
 * mpu6050.c
 *
 *  Created on: 29 oct. 2018
 *      Author: emiliano gonzalez (egonzalez.hiperion@gmail.com)
 */
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "../mpu6050/mpu6050.h"
#include "../mpu6050/inv_mpu.h"
#include "../mpu6050/inv_mpu_dmp_motion_driver.h"

////// mpu6050.h /////
static int luaMPU6050_initialize(lua_State* L) {
    MPU6050_initialize();
    return 0;
}

static int luaMPU6050_testConnection(lua_State* L) {
    lua_pushinteger(L,  MPU6050_testConnection());
    return 1;
}

static int luaMPU6050_Read_Temperature(lua_State* L) {
    lua_pushinteger(L,  Read_Temperature());
    return 1 ;
}

static int luaDMP_Init(lua_State* L) {
    DMP_Init();
    return 0 ;
}

static int luaMPU6050_getMotion6(lua_State* L) {
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    lua_pushinteger(L, ax);
    lua_pushinteger(L, ay);
    lua_pushinteger(L, az);
    lua_pushinteger(L, gx);
    lua_pushinteger(L, gy);
    lua_pushinteger(L, gz);
    return 6;
}

////// inv_mpu.h /////
static int luaMPU6050_mpu_init(lua_State* L) {
    mpu_init();
    return 1;
}

/*
static int luaMPU6050_mpu_init_slave(lua_State* L) {
    mpu_init_slave();
    return 0;
}
*/

static int luaMPU6050_mpu_set_bypass(lua_State* L) {
    lua_pushinteger(L, mpu_set_bypass(luaL_checkinteger(L, 1)));
    return 1;
}

static int luaMPU6050_mpu_lp_accel_mode(lua_State* L) {
    lua_pushinteger(L,  mpu_lp_accel_mode(luaL_checkinteger(L, 1)));
    return 1;
}

static int luaMPU6050_mpu_lp_motion_interrupt(lua_State* L) {
    lua_pushinteger(L,  mpu_lp_motion_interrupt(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2), luaL_checkinteger(L, 3)));
    return 1;
}

static int luaMPU6050_mpu_set_int_level(lua_State* L) {
    lua_pushinteger(L, mpu_set_int_level(luaL_checkinteger(L, 1)));
    return 1;
}

static int luaMPU6050_mpu_set_int_latched(lua_State* L) {
    lua_pushinteger(L, mpu_set_int_latched(luaL_checkinteger(L, 1)));
    return 1;
}

static int luaMPU6050_mpu_set_dmp_state(lua_State* L) {
    lua_pushinteger(L, mpu_set_dmp_state(luaL_checkinteger(L, 1)));
    return 1;
}

static int luaMPU6050_mpu_get_dmp_state(lua_State* L) {
    unsigned char enabled;
    mpu_get_dmp_state(&enabled);
    lua_pushinteger(L, enabled);
    return 1;
}

static int luaMPU6050_mpu_get_lpf(lua_State* L) {
    unsigned short lpf;
    mpu_get_lpf(&lpf);
    lua_pushinteger(L, lpf);
    return 1;
}

static int luaMPU6050_mpu_set_lpf(lua_State* L) {
    unsigned short lpf = luaL_checkinteger(L, 1);
    lua_pushinteger(L, mpu_set_lpf(lpf));
    return 1;
}

static int luaMPU6050_mpu_get_gyro_fsr(lua_State* L) {
    unsigned short fsr;
    mpu_get_gyro_fsr(&fsr);
    lua_pushinteger(L, fsr);
    return 1;
}

static int luaMPU6050_mpu_set_gyro_fsr(lua_State* L) {
    unsigned short fsr = luaL_checkinteger(L, 1);
    lua_pushinteger(L, mpu_set_gyro_fsr(fsr));
    return 1;
}

static int luaMPU6050_mpu_get_accel_fsr(lua_State* L) {
    unsigned char fsr;
    mpu_get_accel_fsr(&fsr);
    lua_pushinteger(L, fsr);
    return 1;
}

static int luaMPU6050_mpu_set_accel_fsr(lua_State* L) {
    unsigned short fsr = luaL_checkinteger(L, 1);
    lua_pushinteger(L,  mpu_set_accel_fsr(fsr));
    return 1;
}

static int luaMPU6050_mpu_get_gyro_sens(lua_State* L) {
    float sens;
    lua_pushinteger(L, mpu_get_gyro_sens(&sens));
    lua_pushnumber(L, sens);
    return 2;
}

static int luaMPU6050_mpu_get_accel_sens(lua_State* L) {
    unsigned short sens ;
    lua_pushinteger(L,mpu_get_accel_sens(&sens));
    lua_pushnumber(L, sens);
    return 2;
}

static int luaMPU6050_mpu_get_sample_rate(lua_State* L) {
    unsigned short rate;
    lua_pushinteger(L, mpu_get_sample_rate(&rate));
    lua_pushnumber(L, rate);
    return 2;
}

static int luaMPU6050_mpu_set_sample_rate(lua_State* L) {
    unsigned short rate = luaL_checkinteger(L, 1);
    lua_pushinteger(L, mpu_set_sample_rate(rate));
    return 1;
}

static int luaMPU6050_mpu_get_fifo_config(lua_State* L) {
    unsigned char sensors;
    lua_pushinteger(L, mpu_get_fifo_config(&sensors));
    lua_pushinteger(L, sensors);
    return 2;
}

static int luaMPU6050_mpu_configure_fifo(lua_State* L) {
    unsigned char sensors = luaL_checkinteger(L, 1);
    lua_pushinteger(L, mpu_configure_fifo(sensors));
    return 1;
}

static int luaMPU6050_mpu_get_power_state(lua_State* L) {
    unsigned char power_on;
    mpu_get_power_state(&power_on);
    lua_pushinteger(L, power_on);
    return 1;
}

static int luaMPU6050_mpu_set_sensors(lua_State* L) {
    unsigned char sensors = luaL_checkinteger(L, 1);
    lua_pushinteger(L, mpu_set_sensors(sensors));
    return 1;
}

//TODO: complete
static int luaMPU6050_mpu_set_accel_bias(lua_State* L) {
    const long accel_bias;
    //lua_pushinteger(L, mpu_set_accel_bias(&accel_bias));
    //lua_pushinteger(L, accel_bias);
    return 0;
}

static int luaMPU6050_mpu_get_gyro_reg(lua_State* L) {
    short data;
    unsigned long timestamp;
    lua_pushinteger(L, mpu_get_gyro_reg(&data, &timestamp));
    lua_pushinteger(L, data);
    lua_pushinteger(L, timestamp);
    return 3;
}

static int luaMPU6050_mpu_get_accel_reg(lua_State* L) {
    short data;
    unsigned long timestamp;
    lua_pushinteger(L, mpu_get_accel_reg(&data, &timestamp));
    lua_pushinteger(L, data);
    lua_pushinteger(L, timestamp);
    return 3;
}

static int luaMPU6050_mpu_get_temperature(lua_State* L) {
    long data;
    unsigned long timestamp = 0;
    lua_pushinteger(L, mpu_get_temperature(&data, &timestamp));
    lua_pushinteger(L, data);
    lua_pushinteger(L, timestamp);
    return 3;
}

static int luaMPU6050_mpu_get_int_status(lua_State* L) {
    short status;
    mpu_get_int_status(&status);
    lua_pushinteger(L, status);
    return 1;
}

static int luaMPU6050_mpu_read_fifo(lua_State* L) {
    short gyro[3];
    short accel[3];
    unsigned long timestamp;
    unsigned char sensors;
    unsigned char more;
    lua_pushinteger(L, mpu_read_fifo(gyro, accel, &timestamp, &sensors, &more));
    lua_pushinteger(L, gyro[0]);
    lua_pushinteger(L, gyro[1]);
    lua_pushinteger(L, gyro[2]);
    lua_pushinteger(L, accel[0]);
    lua_pushinteger(L, accel[1]);
    lua_pushinteger(L, accel[2]);
    lua_pushinteger(L, timestamp);
    lua_pushinteger(L, sensors);
    lua_pushinteger(L, more);
    return 10;
}

static int luaMPU6050_mpu_read_fifo_stream(lua_State* L) {
    unsigned short length = luaL_checkinteger(L, 1);
    unsigned char data;
    unsigned char more;
    lua_pushinteger(L, mpu_read_fifo_stream(length, &data, &more));
    lua_pushinteger(L, data);
    lua_pushinteger(L, more);
    return 3;
}

static int luaMPU6050_mpu_reset_fifo(lua_State* L) {
    lua_pushinteger(L, mpu_reset_fifo());
    return 1;
}

// TODO: Complete
static int luaMPU6050_mpu_write_mem(lua_State* L) {
    unsigned short mem_addr;
    unsigned short length;
    unsigned char data;
    mpu_write_mem(mem_addr, length, &data);
    return 0;
}

// TODO: Complete
static int luaMPU6050_mpu_read_mem(lua_State* L) {
    //mpu_read_mem();
    return 0;
}

// TODO: Complete
static int luaMPU6050_mpu_load_firmware(lua_State* L) {
    unsigned short length;
    const unsigned char firmware;
    unsigned short start_addr;
    unsigned short sample_rate;
    mpu_load_firmware(length, &firmware, start_addr, sample_rate);
    return 0;
}

/*
static int luaMPU6050_mpu_reg_dump(lua_State* L) {
    mpu_reg_dump();
    return 0;
}
*/

static int luaMPU6050_mpu_read_reg(lua_State* L) {
    unsigned char reg = luaL_checkinteger(L, 1);
    unsigned char data;
    lua_pushinteger(L, mpu_read_reg(reg, &data));
    lua_pushinteger(L, data);
    return 2;
}

static int luaMPU6050_mpu_run_self_test(lua_State* L) {
    long gyro;
    long accel;
    lua_pushinteger(L, mpu_run_self_test(&gyro, &accel));
    lua_pushinteger(L, gyro);
    lua_pushinteger(L, accel);
    return 3;
}

/*
static int luaMPU6050_mpu_register_tap_cb(lua_State* L) {
    mpu_register_tap_cb();
    return 0;
}
*/

///// inv_mpu_dmp_motion_driver.h /////
static int luaMPU6050_dmp_load_motion_driver_firmware(lua_State* L) {
    lua_pushinteger(L, dmp_load_motion_driver_firmware());
    return 1;
}

static int luaMPU6050_dmp_set_fifo_rate(lua_State* L) {
    unsigned short rate = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_set_fifo_rate(rate));
    return 1;
}

static int luaMPU6050_dmp_get_fifo_rate(lua_State* L) {
    unsigned short rate;
    lua_pushinteger(L, dmp_get_fifo_rate(&rate));
    lua_pushinteger(L, rate);
    return 2;
}

static int luaMPU6050_dmp_enable_feature(lua_State* L) {
    unsigned short mask = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_enable_feature(mask));
    return 1;
}

static int luaMPU6050_dmp_get_enabled_features(lua_State* L) {
    unsigned short mask;
    dmp_get_enabled_features(&mask);
    lua_pushinteger(L, mask);
    return 1;
}

static int luaMPU6050_dmp_set_interrupt_mode(lua_State* L) {
    unsigned char mode = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_set_interrupt_mode(mode));
    return 1;
}

static int luaMPU6050_dmp_set_orientation(lua_State* L) {
    unsigned short orient = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_set_orientation(orient));
    return 1;
}

// TODO complete
static int luaMPU6050_dmp_set_gyro_bias(lua_State* L) {
    long bias;
    //dmp_set_gyro_bias(&bias);
    return 0;
}

// TODO complete
static int luaMPU6050_dmp_set_accel_bias(lua_State* L) {
    long bias;
    //dmp_set_accel_bias(&bias);
    return 0;
}

// TODO complete
static int luaMPU6050_dmp_register_tap_cb(lua_State* L) {
    //dmp_register_tap_cb();
    return 0;
}

static int luaMPU6050_dmp_set_tap_thresh(lua_State* L) {
    unsigned char axis = luaL_checkinteger(L, 1);
    unsigned short thresh = luaL_checkinteger(L, 2);
    lua_pushinteger(L, dmp_set_tap_thresh(axis, thresh));
    return 1;
}

static int luaMPU6050_dmp_set_tap_axes(lua_State* L) {
    unsigned char axis = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_set_tap_axes(axis));
    return 1;
}

static int luaMPU6050_dmp_set_tap_count(lua_State* L) {
    unsigned char min_taps = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_set_tap_count(min_taps));
    return 1;
}

static int luaMPU6050_dmp_set_tap_time(lua_State* L) {
    unsigned short time = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_set_tap_time(time));
    return 1;
}

static int luaMPU6050_dmp_set_tap_time_multi(lua_State* L) {
    unsigned short time = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_set_tap_time_multi(time));
    return 1;
}

static int luaMPU6050_dmp_set_shake_reject_thresh(lua_State* L) {
    long sf = luaL_checkinteger(L, 1);
    unsigned short thresh = luaL_checkinteger(L, 2);
    lua_pushinteger(L, dmp_set_shake_reject_thresh(sf, thresh));
    return 1;
}

static int luaMPU6050_dmp_set_shake_reject_time(lua_State* L) {
    unsigned short time = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_set_shake_reject_time(time));
    return 1;
}

static int luaMPU6050_dmp_set_shake_reject_timeout(lua_State* L) {
    unsigned short time = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_set_shake_reject_timeout(time));
    return 1;
}

//TODO: complete
static int luaMPU6050_dmp_register_android_orient_cb(lua_State* L) {
    //dmp_register_android_orient_cb();
    return 0;
}

static int luaMPU6050_dmp_enable_lp_quat(lua_State* L) {
    unsigned char enable = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_enable_lp_quat(enable));
    return 1;
}

static int luaMPU6050_dmp_enable_6x_lp_quat(lua_State* L) {
    unsigned char enable = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_enable_6x_lp_quat(enable));
    return 1;
}

static int luaMPU6050_dmp_get_pedometer_step_count(lua_State* L) {
    unsigned long count;
    lua_pushinteger(L, dmp_get_pedometer_step_count(&count));
    lua_pushinteger(L, count);
    return 2;
}

static int luaMPU6050_dmp_set_pedometer_step_count(lua_State* L) {
    unsigned long count = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_set_pedometer_step_count(count));
    return 1;
}

static int luaMPU6050_dmp_get_pedometer_walk_time(lua_State* L) {
    unsigned long time;
    lua_pushinteger(L, dmp_get_pedometer_walk_time(&time));
    lua_pushinteger(L, time);
    return 2;
}

static int luaMPU6050_dmp_set_pedometer_walk_time(lua_State* L) {
    unsigned long time = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_set_pedometer_walk_time(time));
    return 1;
}

static int luaMPU6050_dmp_enable_gyro_cal(lua_State* L) {
    unsigned char enable = luaL_checkinteger(L, 1);
    lua_pushinteger(L, dmp_enable_gyro_cal(enable));
    return 1;
}

static int luaMPU6050_dmp_read_fifo(lua_State* L) {
    uint16_t gyro_val[3];
    uint16_t acc_val[3];
    long int que_val[4];
    long unsigned int timestamp;
    short int sensors[3];
    unsigned char more;
    lua_pushinteger(L, dmp_read_fifo(gyro_val,acc_val,que_val,&timestamp,sensors,&more));
    lua_pushinteger(L, gyro[0]);
    lua_pushinteger(L, gyro[1]);
    lua_pushinteger(L, gyro[2]);
    lua_pushinteger(L, accel[0]);
    lua_pushinteger(L, accel[1]);
    lua_pushinteger(L, accel[2]);
    lua_pushinteger(L, que_val[0]);
    lua_pushinteger(L, que_val[1]);
    lua_pushinteger(L, que_val[2]);
    lua_pushinteger(L, que_val[3]);
    lua_pushinteger(L, timestamp);
    lua_pushinteger(L, sensors[0]);
    lua_pushinteger(L, sensors[1]);
    lua_pushinteger(L, sensors[2]);
    lua_pushinteger(L, more);
    return 16;
}

// TODO: complete
static int luaMPU6050_dmp_get_euler(lua_State* L) {
    //dmp_get_euler();
    return 0;
}

///////////////////// NODEMCU ///////////////////////////
static const LUA_REG_TYPE mpu6050_map[] = {
        // mpu6050.h
        { LSTRKEY( "initialize" ),                      LFUNCVAL( luaMPU6050_initialize )},
        { LSTRKEY( "testConnection" ),                  LFUNCVAL( luaMPU6050_testConnection )},
        { LSTRKEY( "getTemperature" ),                  LFUNCVAL( luaMPU6050_Read_Temperature )},
        { LSTRKEY( "DMPinitialize" ),                   LFUNCVAL( luaDMP_Init )},
        { LSTRKEY( "getMotion6" ),                      LFUNCVAL( luaMPU6050_getMotion6 )},
        //{ LSTRKEY( "getlastMotion6" ),                  LFUNCVAL( luaMPU6050_getlastMotion6 )},
        //{ LSTRKEY( "InitGyroOffset" ),                  LFUNCVAL( luaMPU6050_InitGyro_Offset )},

        // inv_mpu.h
        { LSTRKEY( "mpu_init" ),                        LFUNCVAL( luaMPU6050_mpu_init )},
        //{ LSTRKEY( "mpu_init_slave" ),                  LFUNCVAL( luaMPU6050_mpu_init_slave )},
        { LSTRKEY( "mpu_set_bypass" ),                  LFUNCVAL( luaMPU6050_mpu_set_bypass )},
        { LSTRKEY( "mpu_lp_accel_mode" ),               LFUNCVAL( luaMPU6050_mpu_lp_accel_mode )},
        { LSTRKEY( "mpu_lp_motion_interrupt" ),         LFUNCVAL( luaMPU6050_mpu_lp_motion_interrupt )},
        { LSTRKEY( "mpu_set_int_level" ),               LFUNCVAL( luaMPU6050_mpu_set_int_level )},
        { LSTRKEY( "mpu_set_int_latched" ),             LFUNCVAL( luaMPU6050_mpu_set_int_latched )},
        { LSTRKEY( "mpu_set_dmp_state" ),               LFUNCVAL( luaMPU6050_mpu_set_dmp_state )},
        { LSTRKEY( "mpu_get_dmp_state" ),               LFUNCVAL( luaMPU6050_mpu_get_dmp_state )},
        { LSTRKEY( "mpu_get_lpf" ),                     LFUNCVAL( luaMPU6050_mpu_get_lpf )},
        { LSTRKEY( "mpu_set_lpf" ),                     LFUNCVAL( luaMPU6050_mpu_set_lpf )},
        { LSTRKEY( "mpu_get_gyro_fsr" ),                LFUNCVAL( luaMPU6050_mpu_get_gyro_fsr )},
        { LSTRKEY( "mpu_set_gyro_fsr" ),                LFUNCVAL( luaMPU6050_mpu_set_gyro_fsr )},
        { LSTRKEY( "mpu_get_accel_fsr" ),               LFUNCVAL( luaMPU6050_mpu_get_accel_fsr )},
        { LSTRKEY( "mpu_set_accel_fsr" ),               LFUNCVAL( luaMPU6050_mpu_set_accel_fsr )},
        //{ LSTRKEY( "mpu_get_compass_fsr" ),             LFUNCVAL( luaMPU6050_mpu_get_compass_fsr )},
        { LSTRKEY( "mpu_get_gyro_sens" ),               LFUNCVAL( luaMPU6050_mpu_get_gyro_sens )},
        { LSTRKEY( "mpu_get_accel_sens" ),              LFUNCVAL( luaMPU6050_mpu_get_accel_sens )},
        { LSTRKEY( "mpu_get_sample_rate" ),             LFUNCVAL( luaMPU6050_mpu_get_sample_rate )},
        { LSTRKEY( "mpu_set_sample_rate" ),             LFUNCVAL( luaMPU6050_mpu_set_sample_rate )},
        //{ LSTRKEY( "mpu_get_compass_sample_rate" ),     LFUNCVAL( luaMPU6050_mpu_get_compass_sample_rate )},
        //{ LSTRKEY( "mpu_set_compass_sample_rate" ),     LFUNCVAL( luaMPU6050_mpu_set_compass_sample_rate )},
        { LSTRKEY( "mpu_get_fifo_config" ),             LFUNCVAL( luaMPU6050_mpu_get_fifo_config )},
        { LSTRKEY( "mpu_configure_fifo" ),              LFUNCVAL( luaMPU6050_mpu_configure_fifo )},
        { LSTRKEY( "mpu_get_power_state" ),             LFUNCVAL( luaMPU6050_mpu_get_power_state )},
        { LSTRKEY( "mpu_set_sensors" ),                 LFUNCVAL( luaMPU6050_mpu_set_sensors )},
        { LSTRKEY( "mpu_set_accel_bias" ),              LFUNCVAL( luaMPU6050_mpu_set_accel_bias )},
        { LSTRKEY( "mpu_get_gyro_reg" ),                LFUNCVAL( luaMPU6050_mpu_get_gyro_reg )},
        { LSTRKEY( "mpu_get_accel_reg" ),               LFUNCVAL( luaMPU6050_mpu_get_accel_reg )},
        //{ LSTRKEY( "mpu_get_compass_reg" ),             LFUNCVAL( luaMPU6050_mpu_get_compass_reg )},
        { LSTRKEY( "mpu_get_temperature" ),             LFUNCVAL( luaMPU6050_mpu_get_temperature )},
        { LSTRKEY( "mpu_get_int_status" ),              LFUNCVAL( luaMPU6050_mpu_get_int_status )},
        { LSTRKEY( "mpu_read_fifo" ),                   LFUNCVAL( luaMPU6050_mpu_read_fifo )},
        { LSTRKEY( "mpu_read_fifo_stream" ),            LFUNCVAL( luaMPU6050_mpu_read_fifo_stream )},
        { LSTRKEY( "mpu_reset_fifo" ),                  LFUNCVAL( luaMPU6050_mpu_reset_fifo )},
        { LSTRKEY( "mpu_write_mem" ),                   LFUNCVAL( luaMPU6050_mpu_write_mem )},
        { LSTRKEY( "mpu_read_mem" ),                    LFUNCVAL( luaMPU6050_mpu_read_mem )},
        { LSTRKEY( "mpu_load_firmware" ),               LFUNCVAL( luaMPU6050_mpu_load_firmware )},
        //{ LSTRKEY( "mpu_reg_dump" ),                    LFUNCVAL( luaMPU6050_mpu_reg_dump )},
        { LSTRKEY( "mpu_read_reg" ),                    LFUNCVAL( luaMPU6050_mpu_read_reg )},
        { LSTRKEY( "mpu_run_self_test" ),               LFUNCVAL( luaMPU6050_mpu_run_self_test )},
        //{ LSTRKEY( "mpu_register_tap_cb" ),             LFUNCVAL( luaMPU6050_mpu_register_tap_cb )},

        // inv_mpu_dmp_motion_driver.h
        { LSTRKEY( "dmp_load_motion_driver_firmware" ), LFUNCVAL( luaMPU6050_dmp_load_motion_driver_firmware )},
        { LSTRKEY( "dmp_set_fifo_rate" ),               LFUNCVAL( luaMPU6050_dmp_set_fifo_rate )},
        { LSTRKEY( "dmp_get_fifo_rate" ),               LFUNCVAL( luaMPU6050_dmp_get_fifo_rate )},
        { LSTRKEY( "dmp_enable_feature" ),              LFUNCVAL( luaMPU6050_dmp_enable_feature )},
        { LSTRKEY( "dmp_get_enabled_features" ),        LFUNCVAL( luaMPU6050_dmp_get_enabled_features )},
        { LSTRKEY( "dmp_set_interrupt_mode" ),          LFUNCVAL( luaMPU6050_dmp_set_interrupt_mode )},
        { LSTRKEY( "dmp_set_orientation" ),             LFUNCVAL( luaMPU6050_dmp_set_orientation )},
        { LSTRKEY( "dmp_set_gyro_bias" ),               LFUNCVAL( luaMPU6050_dmp_set_gyro_bias )},
        { LSTRKEY( "dmp_set_accel_bias" ),              LFUNCVAL( luaMPU6050_dmp_set_accel_bias )},
        { LSTRKEY( "dmp_register_tap_cb" ),             LFUNCVAL( luaMPU6050_dmp_register_tap_cb )},
        { LSTRKEY( "dmp_set_tap_thresh" ),              LFUNCVAL( luaMPU6050_dmp_set_tap_thresh )},
        { LSTRKEY( "dmp_set_tap_axes" ),                LFUNCVAL( luaMPU6050_dmp_set_tap_axes )},
        { LSTRKEY( "dmp_set_tap_count" ),               LFUNCVAL( luaMPU6050_dmp_set_tap_count )},
        { LSTRKEY( "dmp_set_tap_time" ),                LFUNCVAL( luaMPU6050_dmp_set_tap_time )},
        { LSTRKEY( "dmp_set_tap_time_multi" ),          LFUNCVAL( luaMPU6050_dmp_set_tap_time_multi )},
        { LSTRKEY( "dmp_set_shake_reject_thresh" ),     LFUNCVAL( luaMPU6050_dmp_set_shake_reject_thresh )},
        { LSTRKEY( "dmp_set_shake_reject_time" ),       LFUNCVAL( luaMPU6050_dmp_set_shake_reject_time )},
        { LSTRKEY( "dmp_set_shake_reject_timeout" ),    LFUNCVAL( luaMPU6050_dmp_set_shake_reject_timeout )},
        { LSTRKEY( "dmp_register_android_orient_cb" ),  LFUNCVAL( luaMPU6050_dmp_register_android_orient_cb )},
        { LSTRKEY( "dmp_enable_lp_quat" ),              LFUNCVAL( luaMPU6050_dmp_enable_lp_quat )},
        { LSTRKEY( "dmp_enable_6x_lp_quat" ),           LFUNCVAL( luaMPU6050_dmp_enable_6x_lp_quat )},
        { LSTRKEY( "dmp_get_pedometer_step_count" ),    LFUNCVAL( luaMPU6050_dmp_get_pedometer_step_count )},
        { LSTRKEY( "dmp_set_pedometer_step_count" ),    LFUNCVAL( luaMPU6050_dmp_set_pedometer_step_count )},
        { LSTRKEY( "dmp_get_pedometer_walk_time" ),     LFUNCVAL( luaMPU6050_dmp_get_pedometer_walk_time )},
        { LSTRKEY( "dmp_set_pedometer_walk_time" ),     LFUNCVAL( luaMPU6050_dmp_set_pedometer_walk_time )},
        { LSTRKEY( "dmp_enable_gyro_cal" ),             LFUNCVAL( luaMPU6050_dmp_enable_gyro_cal )},
        { LSTRKEY( "dmp_read_fifo" ),                   LFUNCVAL( luaMPU6050_dmp_read_fifo )},
        { LSTRKEY( "dmp_get_euler" ),                   LFUNCVAL( luaMPU6050_dmp_get_euler )},

        { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(MPU6050, "mpu6050", mpu6050_map, NULL);
