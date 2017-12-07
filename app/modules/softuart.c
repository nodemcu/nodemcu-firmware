#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "module.h"
#include "c_types.h"
#include "c_string.h"

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"

#define SOFTUART_MAX_RX_BUFF 256

typedef struct softuart_buffer_t {
	char receive_buffer[SOFTUART_MAX_RX_BUFF]; 
	uint8_t length;
} softuart_buffer_t;

typedef struct {
    BOOL enabled;
    uint8_t pin_rx;
    uint8_t pin_tx;
	softuart_buffer_t buffer; //TODO volatile ??? not needed as no read anymore
    uint32_t baudrate;
	uint16_t bit_time;
    uint16_t need_len;
    int16_t end_char;
    int uart_receive_rf;
} Softuart;

Softuart softuart = {false};

//define mapping from pin to functio mode
typedef struct {
	uint32_t gpio_mux_name;
	uint8_t gpio_func;
} softuart_reg_t;

bool softuart_on_data_cb(char *buf, size_t len);

uint8_t softuart_gpio_valid(uint8_t gpio_id)
{
    if ((gpio_id > 5 && gpio_id < 12) || gpio_id > 15)
    {
        return 0;
    }
    return 1;
}

void softuart_set_rx(uint8_t gpio_id)
{
    softuart.pin_rx = gpio_id;
}

void softuart_set_tx(uint8_t gpio_id)
{
    softuart.pin_tx = gpio_id;
}

//TODO ICACHE_RAM_ATTR
static uint32_t softuart_intr_handler(uint32_t ret_gpio_status)
{
    Softuart* s = &softuart;
    uint8_t level;
    uint32_t mask = 1 << s->pin_rx;
    uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);

    // clear gpio status. Say ESP8266EX SDK Programming Guide in  5.1.6. GPIO interrupt handler
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & mask);

    if(gpio_status & mask) {
        gpio_pin_intr_state_set(GPIO_ID_PIN(s->pin_rx), GPIO_PIN_INTR_DISABLE);

        level = GPIO_INPUT_GET(GPIO_ID_PIN(s->pin_rx));
        if(!level) {
            //pin is low
            //therefore we have a start bit

            //wait till start bit is half over so we can sample the next one in the center
            os_delay_us(s->bit_time/2);

            //now sample bits
            unsigned i;
            unsigned d = 0;
            unsigned start_time = 0x7FFFFFFF & system_get_time();

            for(i = 0; i < 8; i ++ )
            {
                while ((0x7FFFFFFF & system_get_time()) < (start_time + (s->bit_time*(i+1))))
                {
                    //If system timer overflow, escape from while loop
                    if ((0x7FFFFFFF & system_get_time()) < start_time){break;}
                }
                d >>= 1;

                //read bit
                if(GPIO_INPUT_GET(GPIO_ID_PIN(s->pin_rx))) {
                    //if high, set msb of 8bit to 1
                    d |= 0x80;
                }
            }

            //store byte in buffer
            uint8 next = s->buffer.length + 1 % SOFTUART_MAX_RX_BUFF;
            if (next != 0)
            {
                s->buffer.receive_buffer[s->buffer.length] = d; // save new byte
                s->buffer.length = next;

                NODE_DBG("SOFTUART char received - new buffer length %d\r\n", s->buffer.length);
                if( ((s->need_len != 0) && (s->buffer.length >= s->need_len)) || \
                    ((s->end_char>=0) && ((unsigned char)d==(unsigned char)s->end_char)))
                {
                    softuart_on_data_cb(s->buffer.receive_buffer, s->buffer.length);
                    s->buffer.length = 0;
                }

            }
            else
            {
                NODE_DBG("SOFTUART buffer overflow\r\n");
            }

            //wait for stop bit
            os_delay_us(s->bit_time);

            //done
        }

        //clear interrupt
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);

        // Reactivate interrupts for GPIO0
        gpio_pin_intr_state_set(GPIO_ID_PIN(s->pin_rx), GPIO_PIN_INTR_ANYEDGE);
    } else {
        //clear interrupt, no matter from which pin
        //otherwise, this interrupt will be called again forever
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);
    }

    ret_gpio_status &= ~(mask);

    return ret_gpio_status;
}

void softuart_init()
{
    Softuart* s = &softuart;

    NODE_DBG("SOFTUART initialize gpio\r\n");
    gpio_init();

	//set bit time
    uint32_t baudrate = s->baudrate;
    s->bit_time = (1000000 / baudrate);
    if (((100000000 / baudrate) - (100*s->bit_time)) > 50 ) s->bit_time++;
    NODE_DBG("SOFTUART bit_time is %d\r\n", s->bit_time);

	//init tx pin
    platform_gpio_mode(s->pin_rx, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_PULLUP);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_tx), 1);
    uint32_t delay = 100000; //TODO needed?
    os_delay_us(delay);
    NODE_DBG("SOFTUART TX INIT DONE\r\n");

    //init rx pin
    platform_gpio_mode(s->pin_rx, PLATFORM_GPIO_INT, PLATFORM_GPIO_PULLUP);
    uint32_t mask = 1 << s->pin_rx;
    platform_gpio_register_intr_hook(mask, softuart_intr_handler);

    //clear interrupt handler status, basically writing a low to the output
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(s->pin_rx)); //TODO needed?

    //enable interrupt for pin on any edge (rise and fall)
    gpio_pin_intr_state_set(GPIO_ID_PIN(s->pin_rx), GPIO_PIN_INTR_ANYEDGE);
    NODE_DBG("SOFTUART RX INIT DONE\r\n");
}

static inline u8 chbit(u8 data, u8 bit)
{
    if ((data & bit) != 0)
    {
    	return 1;
    }
    else
    {
    	return 0;
    }
}

// Function for printing individual characters
void softuart_putchar(char data)
{
    NODE_DBG("putchar called\r\n");
    Softuart* s = &softuart;
	unsigned i;
	unsigned start_time = 0x7FFFFFFF & system_get_time();

	//Start Bit
	GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_tx), 0); //TODO replay by plaform function
	for(i = 0; i < 8; i ++ )
	{
		while ((0x7FFFFFFF & system_get_time()) < (start_time + (s->bit_time*(i+1))))
		{
			//If system timer overflow, escape from while loop
			if ((0x7FFFFFFF & system_get_time()) < start_time){break;}
		}
		GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_tx), chbit(data,1<<i));
	}

	// Stop bit
	while ((0x7FFFFFFF & system_get_time()) < (start_time + (s->bit_time*9)))
	{
		//If system timer overflow, escape from while loop
		if ((0x7FFFFFFF & system_get_time()) < start_time){break;}
	}
	GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_tx), 1);

	// Delay after byte, for new sync
	os_delay_us(s->bit_time*6);
}


static int softuart_setup(lua_State* L)
{
    uint32_t result = 1;
    uint32_t baudrate = (uint32_t) luaL_checkinteger(L, 1);
    uint8_t tx_gpio_id = (uint8_t) luaL_checkinteger(L, 2);
    uint8_t rx_gpio_id = (uint8_t) luaL_checkinteger(L, 3);

    if(softuart.enabled)
        return luaL_error( L, "SOFTUART already configured." );

    if(!softuart_gpio_valid(tx_gpio_id)) {
        return luaL_error(L, "SOFTUART tx GPIO not valid");
    }
    softuart_set_tx(tx_gpio_id);
    if(!softuart_gpio_valid(rx_gpio_id)) {
        return luaL_error(L, "SOFTUART rx GPIO not valid");
    }
    softuart_set_rx(rx_gpio_id);

    if (baudrate <= 0 || baudrate > 38400)
    {
        return luaL_error(L, "SOFTUART invalid baud rate" );
    }
    softuart.baudrate = baudrate;

    softuart_init();

    softuart.uart_receive_rf = LUA_NOREF;
    softuart.enabled = true;

    lua_pushnumber(L, (lua_Number)result);
    return 1;
}

static int softuart_write(lua_State* L)
{
    const char* buf;
    size_t len, i;

    if(!softuart.enabled)
        return luaL_error( L, "Call setup first" );

    int total = lua_gettop(L);

    for(int s = 1; s <= total; s ++)
    {
        if(lua_type( L, s ) == LUA_TNUMBER)
        {
            int val = lua_tointeger(L, s);
            if(len > 255)
                return luaL_error(L, "invalid value");
            softuart_putchar((u8)val);
        }
        else
        {
            luaL_checktype(L, s, LUA_TSTRING);
            buf = lua_tolstring(L, s, &len);
            for(i = 0; i < len; i ++)
                softuart_putchar(buf[i]);
        }
    }
    return 0;
}

static int softuart_on(lua_State* L)
{
    size_t sl, el;
    uint8_t stack = 1;
    softuart.need_len = 1;

    if(!softuart.enabled)
        return luaL_error( L, "Call setup first" );

    const char *method = luaL_checklstring( L, stack, &sl );
    if (method == NULL)
        return luaL_error( L, "wrong arg type" );
    stack++;

    if( lua_type( L, stack ) == LUA_TNUMBER )
    {
        softuart.need_len = ( uint16_t )luaL_checkinteger( L, stack );
        stack++;
        softuart.end_char = -1;
        if( softuart.need_len > 255 )
        {
            softuart.need_len = 255;
            return luaL_error( L, "wrong arg range" );
        }
    }
    else if(lua_isstring(L, stack))
    {
        const char *end = luaL_checklstring( L, stack, &el );
        stack++;
        if(el!=1)
        {
            return luaL_error( L, "wrong end char length" );
        }
        softuart.end_char = (int16_t)end[0];
        softuart.need_len = 0;
    }

    if (lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION)
    {
        lua_pushvalue(L, stack);  // copy argument (func) to the top of stack
    }
    else
    {
        lua_pushnil(L);
    }

    if(sl == 4 && c_strcmp(method, "data") == 0)
    {
        if(softuart.uart_receive_rf != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, softuart.uart_receive_rf);
            softuart.uart_receive_rf = LUA_NOREF;
        }
        if(!lua_isnil(L, -1))
        {
            softuart.uart_receive_rf = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            lua_pop(L, 1);
        }
    }
    else
    {
        lua_pop(L, 1);
        return luaL_error( L, "method not supported" );
    }
    return 0;
}

bool softuart_on_data_cb(char *buf, size_t len)
{
    if(!buf || len==0)
        return false;
    if(softuart.uart_receive_rf == LUA_NOREF)
        return false;
    lua_State *L = lua_getstate();
    if(!L)
        return false;
    lua_rawgeti(L, LUA_REGISTRYINDEX, softuart.uart_receive_rf);
    lua_pushlstring(L, buf, len);
    lua_call(L, 1, 0);
    return true;
}

// uart.getconfig(id)
static int softuart_getconfig(lua_State* L)
{
    uint32_t databits = 8, parity = 0, stopbits = 1;

    if(!softuart.enabled)
        return luaL_error( L, "Call setup first" );

    lua_pushinteger(L, softuart.baudrate);
    lua_pushinteger(L, databits);
    lua_pushinteger(L, parity);
    lua_pushinteger(L, stopbits);
    return 4;
}

const LUA_REG_TYPE softuart_map[] =
{
  { LSTRKEY( "setup" ), LFUNCVAL( softuart_setup ) },
  { LSTRKEY( "write" ), LFUNCVAL( softuart_write ) },
  { LSTRKEY( "softuart_getconfig" ), LFUNCVAL( softuart_getconfig ) },
  { LSTRKEY( "on" ), LFUNCVAL( softuart_on ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(SOFTUART, "softuart", softuart_map, NULL);
