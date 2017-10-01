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

//array of pointers to instances

typedef struct softuart_pin_t {
	uint8_t gpio_id;
	uint32_t gpio_mux_name;
	uint8_t gpio_func;
} softuart_pin_t;

typedef struct softuart_buffer_t {
	char receive_buffer[SOFTUART_MAX_RX_BUFF]; 
	uint8_t length;
} softuart_buffer_t;

typedef struct {
    BOOL enabled;
	softuart_pin_t pin_rx;
	softuart_pin_t pin_tx;
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

softuart_reg_t softuart_reg[] =
{
	{ PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0 },	//gpio0
	{ PERIPHS_IO_MUX_U0TXD_U, FUNC_GPIO1 },	//gpio1 (uart)
	{ PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2 },	//gpio2
	{ PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3 },	//gpio3 (uart)
	{ PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4 },	//gpio4
	{ PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5 },	//gpio5
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12 },	//gpio12
	{ PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13 },	//gpio13
	{ PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14 },	//gpio14
	{ PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15 },	//gpio15
	//@TODO TODO gpio16 is missing (?include)
};

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
    softuart.pin_rx.gpio_id = gpio_id;
    softuart.pin_rx.gpio_mux_name = softuart_reg[gpio_id].gpio_mux_name;
    softuart.pin_rx.gpio_func = softuart_reg[gpio_id].gpio_func;
}

void softuart_set_tx(uint8_t gpio_id)
{
    softuart.pin_tx.gpio_id = gpio_id;
    softuart.pin_tx.gpio_mux_name = softuart_reg[gpio_id].gpio_mux_name;
    softuart.pin_tx.gpio_func = softuart_reg[gpio_id].gpio_func;
}

uint8_t softuart_bitcount(uint32_t x)
{
	uint8_t count;
 
	for (count=0; x != 0; x>>=1) {
		if ( x & 0x01) {
			return count;
		}
		count++;
	}
	//error: no 1 found!
	return 0xFF;
}

void softuart_intr_handler()
{
    Softuart* s = &softuart;
    uint8_t level, gpio_id;
    // clear gpio status. Say ESP8266EX SDK Programming Guide in  5.1.6. GPIO interrupt handler

    uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
    gpio_id = softuart_bitcount(gpio_status);

    //if interrupt was by an attached rx pin
    if (gpio_id != 0xFF)
    {
        // disable interrupt for GPIO0
        gpio_pin_intr_state_set(GPIO_ID_PIN(s->pin_rx.gpio_id), GPIO_PIN_INTR_DISABLE);
        level = GPIO_INPUT_GET(GPIO_ID_PIN(s->pin_rx.gpio_id));
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
                //shift d to the right
                d >>= 1;

                //read bit
                if(GPIO_INPUT_GET(GPIO_ID_PIN(s->pin_rx.gpio_id))) {
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
        gpio_pin_intr_state_set(GPIO_ID_PIN(s->pin_rx.gpio_id), 3);
    } else {
        //clear interrupt, no matter from which pin
        //otherwise, this interrupt will be called again forever
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);
    }
}

void softuart_init(uint32_t baudrate)
{
    Softuart* s = &softuart;

    NODE_DBG("SOFTUART initialize gpio\r\n");
    gpio_init();

	//set bit time
    s->bit_time = (1000000 / baudrate);
    if ( ((100000000 / baudrate) - (100*s->bit_time)) > 50 ) s->bit_time++;
    NODE_DBG("SOFTUART bit_time is %d\r\n",s->bit_time);

    s->baudrate = baudrate;

	//init tx pin
	if(!s->pin_tx.gpio_mux_name) {
		NODE_DBG("SOFTUART ERROR: Set tx pin (%d)\r\n",s->pin_tx.gpio_mux_name);
	} else {
		//enable pin as gpio
    	PIN_FUNC_SELECT(s->pin_tx.gpio_mux_name, s->pin_tx.gpio_func);

		//set pullup (UART idle is VDD)
		PIN_PULLUP_EN(s->pin_tx.gpio_mux_name);
		
		//set high for tx idle
		GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_tx.gpio_id), 1);
        uint32_t delay = 100000;
		os_delay_us(delay);

		NODE_DBG("SOFTUART TX INIT DONE\r\n");
	}

	//init rx pin
	if(!s->pin_rx.gpio_mux_name) {
		NODE_DBG("SOFTUART ERROR: Set rx pin (%d)\r\n",s->pin_rx.gpio_mux_name);
	} else {
		//enable pin as gpio
    	PIN_FUNC_SELECT(s->pin_rx.gpio_mux_name, s->pin_rx.gpio_func);

		//set pullup (UART idle is VDD)
		PIN_PULLUP_EN(s->pin_rx.gpio_mux_name);
		
		//set to input -> disable output
		GPIO_DIS_OUTPUT(GPIO_ID_PIN(s->pin_rx.gpio_id));

		//set interrupt related things

		//disable interrupts by GPIO
		ETS_GPIO_INTR_DISABLE();

		//attach interrupt handler and a pointer that will be passed around each time
		ETS_GPIO_INTR_ATTACH((ets_isr_t)softuart_intr_handler, NULL);

		gpio_register_set(GPIO_PIN_ADDR(s->pin_rx.gpio_id), GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)  |
                           GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE) |
                           GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
		
		//clear interrupt handler status, basically writing a low to the output
		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(s->pin_rx.gpio_id));

		//enable interrupt for pin on any edge (rise and fall)
		//@TODO: should work with ANYEDGE (=3), but complie error
		gpio_pin_intr_state_set(GPIO_ID_PIN(s->pin_rx.gpio_id), 3);

		//globally enable GPIO interrupts
		ETS_GPIO_INTR_ENABLE();
	
		NODE_DBG("SOFTUART RX INIT DONE\r\n");
	}

	NODE_DBG("SOFTUART INIT DONE\r\n");
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
	GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_tx.gpio_id), 0);
	for(i = 0; i < 8; i ++ )
	{
		while ((0x7FFFFFFF & system_get_time()) < (start_time + (s->bit_time*(i+1))))
		{
			//If system timer overflow, escape from while loop
			if ((0x7FFFFFFF & system_get_time()) < start_time){break;}
		}
		GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_tx.gpio_id), chbit(data,1<<i));
	}

	// Stop bit
	while ((0x7FFFFFFF & system_get_time()) < (start_time + (s->bit_time*9)))
	{
		//If system timer overflow, escape from while loop
		if ((0x7FFFFFFF & system_get_time()) < start_time){break;}
	}
	GPIO_OUTPUT_SET(GPIO_ID_PIN(s->pin_tx.gpio_id), 1);

	// Delay after byte, for new sync
	os_delay_us(s->bit_time*6);
}


static int softuart_setup(lua_State* L)
{
    uint32_t result = 1;
    uint32_t baudrate = (uint32_t) luaL_checkinteger(L, 1);
    uint8_t tx_gpio_id = (uint8_t) luaL_checkinteger(L, 2);
    uint8_t rx_gpio_id = (uint8_t) luaL_checkinteger(L, 3);

    if(!softuart_gpio_valid(tx_gpio_id)) {
        return luaL_error(L, "SOFTUART tx GPIO not valid");
    }
    softuart_set_rx(tx_gpio_id);
    if(!softuart_gpio_valid(rx_gpio_id)) {
        return luaL_error(L, "SOFTUART rx GPIO not valid");
    }
    softuart_set_tx(rx_gpio_id);

    if (baudrate <= 0 || baudrate > 38400)
    {
        return luaL_error(L, "SOFTUART invalid baud rate" );
    }
    softuart_init(baudrate);

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
