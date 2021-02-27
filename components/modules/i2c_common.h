
#ifndef _NODEMCU_I2C_COMMON_H_
#define _NODEMCU_I2C_COMMON_H_

#include "lauxlib.h"


typedef enum {
  I2C_ID_SW = 0,
  I2C_ID_HW0,
  I2C_ID_HW1,
  I2C_ID_MAX
} i2c_id_type;


// ***************************************************************************
// Hardware master prototypes
//
void li2c_hw_master_init( lua_State *L );
int li2c_hw_master_setup( lua_State *L, unsigned id, unsigned sda, unsigned scl, uint32_t speed, unsigned stretchfactor );
void li2c_hw_master_start( lua_State *L, unsigned id );
void li2c_hw_master_stop( lua_State *L, unsigned id );
int li2c_hw_master_address( lua_State *L, unsigned id, uint16_t address, uint8_t direction, bool ack_check_en );
void li2c_hw_master_write( lua_State *L, unsigned id, uint8_t data, bool ack_check_en );
void li2c_hw_master_read( lua_State *L, unsigned id, uint32_t len );
int li2c_hw_master_transfer( lua_State *L );


// ***************************************************************************
// Hardware slave prototypes
//
LROT_EXTERN(li2c_slave);
void li2c_hw_slave_init( lua_State *L );



#endif /*_NODEMCU_I2C_COMMON_H_*/
