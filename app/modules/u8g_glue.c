/*
  Functions for integrating U8glib into the nodemcu platform.
*/

#include "lauxlib.h"
#include "platform.h"

#include "c_stdlib.h"

#include "u8g.h"
#include "u8g_glue.h"


// ------------------------------------------------------------
// comm functions
//
#define I2C_CMD_MODE    0x000
#define I2C_DATA_MODE   0x040

#define ESP_I2C_ID 0


static uint8_t do_i2c_start(uint8_t id, uint8_t sla)
{
    platform_i2c_send_start( id );

    // ignore return value -> tolerate missing ACK
    platform_i2c_send_address( id, sla, PLATFORM_I2C_DIRECTION_TRANSMITTER );

    return 1;
}

static uint8_t u8g_com_esp8266_ssd_start_sequence(struct _lu8g_userdata_t *lu8g)
{
    /* are we requested to set the a0 state? */
    if ( lu8g->u8g.pin_list[U8G_PI_SET_A0] == 0 )
        return 1;

    /* setup bus, might be a repeated start */
    if ( do_i2c_start( ESP_I2C_ID, lu8g->i2c_addr ) == 0 )
        return 0;
    if ( lu8g->u8g.pin_list[U8G_PI_A0_STATE] == 0 )
    {
        // ignore return value -> tolerate missing ACK
        if ( platform_i2c_send_byte( ESP_I2C_ID, I2C_CMD_MODE ) == 0 )
            ; //return 0;
    }
    else
    {
        platform_i2c_send_byte( ESP_I2C_ID, I2C_DATA_MODE );
    }

    lu8g->u8g.pin_list[U8G_PI_SET_A0] = 0;
    return 1;
}


static void lu8g_digital_write( struct _lu8g_userdata_t *lu8g, uint8_t pin_index, uint8_t value )
{
    uint8_t pin;

    pin = lu8g->u8g.pin_list[pin_index];
    if ( pin != U8G_PIN_NONE )
        platform_gpio_write( pin, value );
}

void u8g_Delay(u8g_t *u8g, uint16_t msec)
{
    struct _lu8g_userdata_t *lu8g = (struct _lu8g_userdata_t *)u8g;
    const uint16_t chunk = 50;

    if (lu8g->use_delay == 0)
        return;

    while (msec > chunk)
    {
        os_delay_us( chunk*1000 );
        msec -= chunk;
    }
    if (msec > 0)
        os_delay_us( msec*1000 );
}
void u8g_MicroDelay(void)
{
    os_delay_us( 1 );
}
void u8g_10MicroDelay(void)
{
    os_delay_us( 10 );
}


uint8_t u8g_com_esp8266_hw_spi_fn(u8g_t *u8g, uint8_t msg, uint8_t arg_val, void *arg_ptr)
{
    struct _lu8g_userdata_t *lu8g = (struct _lu8g_userdata_t *)u8g;

    switch(msg)
    {
    case U8G_COM_MSG_STOP:
        break;
    
    case U8G_COM_MSG_INIT:
        // we assume that the SPI interface was already initialized
        // just care for the /CS and D/C pins
        lu8g_digital_write( lu8g, U8G_PI_CS, PLATFORM_GPIO_HIGH );
        platform_gpio_mode( lu8g->u8g.pin_list[U8G_PI_CS], PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT );
        platform_gpio_mode( lu8g->u8g.pin_list[U8G_PI_A0], PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT );
        break;
    
    case U8G_COM_MSG_ADDRESS:                     /* define cmd (arg_val = 0) or data mode (arg_val = 1) */
        lu8g_digital_write( lu8g, U8G_PI_A0, arg_val == 0 ? PLATFORM_GPIO_LOW : PLATFORM_GPIO_HIGH );
        break;

    case U8G_COM_MSG_CHIP_SELECT:
        if (arg_val == 0)
        {
            /* disable */
            lu8g_digital_write( lu8g, U8G_PI_CS, PLATFORM_GPIO_HIGH );
        }
        else
        {
            /* enable */
            lu8g_digital_write( lu8g, U8G_PI_CS, PLATFORM_GPIO_LOW );
        }
        break;
      
    case U8G_COM_MSG_RESET:
        if ( lu8g->u8g.pin_list[U8G_PI_RESET] != U8G_PIN_NONE )
            lu8g_digital_write( lu8g, U8G_PI_RESET, arg_val == 0 ? PLATFORM_GPIO_LOW : PLATFORM_GPIO_HIGH );
        break;
    
    case U8G_COM_MSG_WRITE_BYTE:
        platform_spi_send( 1, 8, arg_val );
        break;
    
    case U8G_COM_MSG_WRITE_SEQ:
    case U8G_COM_MSG_WRITE_SEQ_P:
        {
            register uint8_t *ptr = arg_ptr;
            while( arg_val > 0 )
            {
                platform_spi_send( 1, 8, *ptr++ );
                arg_val--;
            }
        }
        break;
    }
    return 1;
}


uint8_t u8g_com_esp8266_ssd_i2c_fn(u8g_t *u8g, uint8_t msg, uint8_t arg_val, void *arg_ptr)
{
    struct _lu8g_userdata_t *lu8g = (struct _lu8g_userdata_t *)u8g;

    switch(msg)
    {
    case U8G_COM_MSG_INIT:
        // we assume that the i2c bus was already initialized
        //u8g_i2c_init(u8g->pin_list[U8G_PI_I2C_OPTION]);

        break;
    
    case U8G_COM_MSG_STOP:
        break;

    case U8G_COM_MSG_RESET:
        /* Currently disabled, but it could be enable. Previous restrictions have been removed */
        /* u8g_com_arduino_digital_write(u8g, U8G_PI_RESET, arg_val); */
        break;
      
    case U8G_COM_MSG_CHIP_SELECT:
        lu8g->u8g.pin_list[U8G_PI_A0_STATE] = 0;
        lu8g->u8g.pin_list[U8G_PI_SET_A0] = 1;		/* force a0 to set again, also forces start condition */
        if ( arg_val == 0 )
        {
            /* disable chip, send stop condition */
            platform_i2c_send_stop( ESP_I2C_ID );
        }
        else
        {
            /* enable, do nothing: any byte writing will trigger the i2c start */
        }
        break;

    case U8G_COM_MSG_WRITE_BYTE:
        //u8g->pin_list[U8G_PI_SET_A0] = 1;
        if ( u8g_com_esp8266_ssd_start_sequence(lu8g) == 0 )
            return platform_i2c_send_stop( ESP_I2C_ID ), 0;
        // ignore return value -> tolerate missing ACK
        if ( platform_i2c_send_byte( ESP_I2C_ID, arg_val) == 0 )
            ; //return platform_i2c_send_stop( ESP_I2C_ID ), 0;
        // platform_i2c_send_stop( ESP_I2C_ID );
        break;
    
    case U8G_COM_MSG_WRITE_SEQ:
    case U8G_COM_MSG_WRITE_SEQ_P:
        //u8g->pin_list[U8G_PI_SET_A0] = 1;
        if ( u8g_com_esp8266_ssd_start_sequence(lu8g) == 0 )
            return platform_i2c_send_stop( ESP_I2C_ID ), 0;
        {
            register uint8_t *ptr = arg_ptr;
            while( arg_val > 0 )
            {
                // ignore return value -> tolerate missing ACK
                if ( platform_i2c_send_byte( ESP_I2C_ID, *ptr++) == 0 )
                    ; //return platform_i2c_send_stop( ESP_I2C_ID ), 0;
                arg_val--;
            }
        }
        // platform_i2c_send_stop( ESP_I2C_ID );
        break;

    case U8G_COM_MSG_ADDRESS:                     /* define cmd (arg_val = 0) or data mode (arg_val = 1) */
        lu8g->u8g.pin_list[U8G_PI_A0_STATE] = arg_val;
        lu8g->u8g.pin_list[U8G_PI_SET_A0] = 1;		/* force a0 to set again */
    
        break;
    }
    return 1;
}


// ***************************************************************************
// Generic framebuffer device and RLE comm driver
//
uint8_t u8g_dev_gen_fb_fn(u8g_t *u8g, u8g_dev_t *dev, uint8_t msg, void *arg)
{
  switch(msg)
  {
    case U8G_DEV_MSG_PAGE_FIRST:
      // tell comm driver to start new framebuffer
      u8g_SetChipSelect(u8g, dev, 1);
      break;
    case U8G_DEV_MSG_PAGE_NEXT:
      {
        u8g_pb_t *pb = (u8g_pb_t *)(dev->dev_mem);
        if ( u8g_pb_WriteBuffer(pb, u8g, dev) == 0 )
          return 0;
      }
      break;
  }

  return u8g_dev_pb8v1_base_fn(u8g, dev, msg, arg);
}

static int bit_at( uint8_t *buf, int line, int x )
{
    uint8_t byte = buf[x];

    return buf[x] & (1 << line) ? 1 : 0;
}

struct _lu8g_fbrle_item
{
    uint8_t start_x;
    uint8_t len;
};

struct _lu8g_fbrle_line
{
    uint8_t num_valid;
    struct _lu8g_fbrle_item items[0];
};

uint8_t u8g_com_esp8266_fbrle_fn(u8g_t *u8g, uint8_t msg, uint8_t arg_val, void *arg_ptr)
{
    struct _lu8g_userdata_t *lud = (struct _lu8g_userdata_t *)u8g;

    switch(msg)
    {
    case U8G_COM_MSG_INIT:
        // allocate memory -> done
        // init buffer
        break;

    case U8G_COM_MSG_CHIP_SELECT:
        if (arg_val == 1) {
            // new frame starts
            if (lud->cb_ref != LUA_NOREF) {
                // fire callback with nil argument
                lua_State *L = lua_getstate();
                lua_rawgeti( L, LUA_REGISTRYINDEX, lud->cb_ref );
                lua_pushnil( L );
                lua_call( L, 1, 0 );
            }
        }
        break;

    case U8G_COM_MSG_WRITE_SEQ:
    case U8G_COM_MSG_WRITE_SEQ_P:
        {
            uint8_t xwidth = u8g->pin_list[U8G_PI_EN];
            size_t fbrle_line_size = sizeof( struct _lu8g_fbrle_line ) + sizeof( struct _lu8g_fbrle_item ) * (xwidth/2);
            int num_lines = arg_val / (xwidth/8);
            uint8_t *buf = (uint8_t *)arg_ptr;

            struct _lu8g_fbrle_line *fbrle_line;
            if (!(fbrle_line = (struct _lu8g_fbrle_line *)c_malloc( fbrle_line_size ))) {
                break;
            }

            for (int line = 0; line < num_lines; line++) {
                int start_run = -1;
                fbrle_line->num_valid = 0;

                for (int x = 0; x < xwidth; x++) {
                    if (bit_at( buf, line, x ) == 0) {
                        if (start_run >= 0) {
                            // inside run, end it and enter result
                            fbrle_line->items[fbrle_line->num_valid].start_x = start_run;
                            fbrle_line->items[fbrle_line->num_valid++].len = x - start_run;
                            //NODE_ERR( "         line: %d x: %d len: %d\n", line, start_run, x - start_run );
                            start_run = -1;
                        }
                    } else {
                        if (start_run < 0) {
                            // outside run, start it
                            start_run = x;
                        }
                    }

                    if (fbrle_line->num_valid >= xwidth/2) break;
                }

                // active run?
                if (start_run >= 0 && fbrle_line->num_valid < xwidth/2) {
                    fbrle_line->items[fbrle_line->num_valid].start_x = start_run;
                    fbrle_line->items[fbrle_line->num_valid++].len = xwidth - start_run;
                }

                // line done, trigger callback
                if (lud->cb_ref != LUA_NOREF) {
                    lua_State *L = lua_getstate();

                    lua_rawgeti( L, LUA_REGISTRYINDEX, lud->cb_ref );
                    lua_pushlstring( L, (const char *)fbrle_line, fbrle_line_size );
                    lua_call( L, 1, 0 );
                }
            }

            c_free( fbrle_line );
        }
        break;
    }
    return 1;
}
