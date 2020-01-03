#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#include "module.h"
#include "lauxlib.h"
#include "task/task.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

#define SOFTUART_MAX_RX_BUFF 128
#define SOFTUART_GPIO_COUNT 13

typedef struct {
	char receive_buffer[SOFTUART_MAX_RX_BUFF];
	uint8_t length;
	uint8_t buffer_overflow;
} softuart_buffer_t;

typedef struct {
	uint8_t pin_rx;
	uint8_t pin_tx;
	volatile softuart_buffer_t buffer;
	uint16_t bit_time;
	uint16_t need_len; // Buffer length needed to run callback function
	char end_char; // Used to run callback if last char in buffer will be the same
	uint8_t armed;
} softuart_t;

typedef struct {
	softuart_t *softuart;
} softuart_userdata;

// Array of pointers to SoftUART instances
softuart_t * softuart_gpio_instances[SOFTUART_GPIO_COUNT] = {NULL};
// Array of callback reference to be able to find which callback is used to which rx pin
static int softuart_rx_cb_ref[SOFTUART_GPIO_COUNT];
// Task for receiving data
static task_handle_t uart_recieve_task = 0;
// Receiving buffer for callback usage
static char softuart_rx_buffer[SOFTUART_MAX_RX_BUFF];

static inline int32_t asm_ccount(void) {
    int32_t r;
    asm volatile ("rsr %0, ccount" : "=r"(r));
    return r;
}

static inline uint8_t checkbit(uint8_t data, uint8_t bit)
{
	if ((data & bit) != 0) {
		return 1;
	} else {
		return 0;
	}
}

uint32_t ICACHE_RAM_ATTR softuart_intr_handler(uint32_t ret_gpio_status)
{
	// Disable all interrupts
	ets_intr_lock();
	int32_t start_time = asm_ccount();
	uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	uint32_t gpio_bits = gpio_status;
	for (uint8_t gpio_bit = 0; gpio_bits != 0; gpio_bit++, gpio_bits >>= 1) {
		// Check all pins for interrupts
		if (! (gpio_bits & 0x01)) continue;
		// We got pin that was interrupted
		// Load instance which has rx pin on interrupt pin attached
		softuart_t *s = softuart_gpio_instances[pin_num_inv[gpio_bit]];
		if (s == NULL) continue;
		if (softuart_rx_cb_ref[pin_num_inv[gpio_bit]] == LUA_NOREF) continue;
		if (!s->armed) continue;
		// There is SoftUART rx instance on that pin
		// Clear interrupt status on that pin
		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & (1 << pin_num[s->pin_rx]));
		ret_gpio_status &= ~(1 << pin_num[s->pin_rx]);
		// Start listening to transmission
		// TODO: inverted
		if (! (GPIO_INPUT_GET(GPIO_ID_PIN(pin_num[s->pin_rx])))) {
			//pin is low - therefore we have a start bit
			unsigned byte = 0;
			// Casting and using signed types to always be able to compute elapsed time even if there is a overflow
			uint32_t elapsed_time = (uint32_t)(asm_ccount() - start_time);

			// Wait till start bit is half over so we can sample the next one in the center
			if (elapsed_time < s->bit_time / 2) {
				uint16_t wait_time = s->bit_time / 2 - elapsed_time;
				while ((uint32_t)(asm_ccount() - start_time) < wait_time);
				start_time += wait_time;
			}

			// Sample bits
			// TODO: How many bits? Add other configs to softuart
			for (uint8_t i = 0; i < 8; i ++ ) {
				while ((uint32_t)(asm_ccount() - start_time) < s->bit_time);
				//shift d to the right
				byte >>= 1;

				// Read bit
				if(GPIO_INPUT_GET(GPIO_ID_PIN(pin_num[s->pin_rx]))) {
					// If high, set msb of 8bit to 1
					byte |= 0x80;
				}
				// Recalculate start time for next bit
				start_time += s->bit_time;
			}

			// Wait for stop bit
			// TODO: Add config for stop bits and parity bits
			while ((uint32_t)(asm_ccount() - start_time) < s->bit_time);

			// Store byte in buffer
			// If buffer full, set the overflow flag and return
			uint8 next = s->buffer.length + 1 % SOFTUART_MAX_RX_BUFF;
			if (next != 0) {
				s->buffer.receive_buffer[s->buffer.length] = byte; // save new byte
					s->buffer.length = next;
					// Run callback when buffer is filled with enough data or last char is the triggering one
					if (((s->need_len != 0) && (s->buffer.length >= s->need_len))  || \
					((s->need_len == 0) && ((char)byte == s->end_char))) {
						s->armed = 0;
						task_post_low(uart_recieve_task, (task_param_t)s); // Send the pointer to task handler
					}
			} else {
				//TODO: use this information somehow?
				s->buffer.buffer_overflow = 1;
			}
		}
	}
	// re-enable all interrupts
	ets_intr_unlock();
	return ret_gpio_status;
}

static void softuart_putchar(softuart_t *s, char data)
{
	// Disable all interrupts
	ets_intr_lock();
	int32_t start_time = asm_ccount();
	// Set start bit
	GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[s->pin_tx]), 0);
	for (uint32_t i = 0; i < 8; i++) {
		while ((uint32_t)(asm_ccount() - start_time) < s->bit_time);

		GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[s->pin_tx]), checkbit(data, 1 << i));
		// Recalculate start time for next bit
		start_time += s->bit_time;
	}

	// Stop bit
	while ((uint32_t)(asm_ccount() - start_time) < s->bit_time);
	GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[s->pin_tx]), 1);
	// Delay after byte, for new sync
	os_delay_us(s->bit_time*6 / system_get_cpu_freq());
	// Re-enable all interrupts
	ets_intr_unlock();
}

static void softuart_init(softuart_t *s)
{
    NODE_DBG("SoftUART initialize gpio\n");

    if (s->pin_tx != 0xFF){
    	// Init tx pin
        platform_gpio_mode(s->pin_tx, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_PULLUP);
        platform_gpio_write(s->pin_tx, PLATFORM_GPIO_HIGH);
    }

    // Init rx pin
    if (s->pin_rx != 0xFF){
		platform_gpio_mode(s->pin_rx, PLATFORM_GPIO_INT, PLATFORM_GPIO_PULLUP);
		uint32_t mask = 1 << pin_num[s->pin_rx];
		platform_gpio_register_intr_hook(mask, softuart_intr_handler);

		softuart_gpio_instances[s->pin_rx] = s;
		// Enable interrupt for pin on falling edge
		platform_gpio_intr_init(s->pin_rx, GPIO_PIN_INTR_NEGEDGE);
    }
}


static int softuart_setup(lua_State *L)
{
	uint32_t baudrate;
	uint8_t tx_gpio_id, rx_gpio_id;
	uint8_t stack = 1;
	softuart_userdata *suart = NULL;

	NODE_DBG("SoftUART setup called\n");

	if(lua_isnumber(L, stack)) {
		baudrate = (uint32_t)luaL_checkinteger( L, stack);
	    //230400 Is the max baudrate the author of Arduino-Esp8266-Software-UART tested
	    if (baudrate <= 0 || baudrate > 230400) {
	        return luaL_error(L, "Invalid baud rate" );
	    }
	    stack++;
	} else {
		return luaL_error(L, "Invalid argument type");
	}

	if(lua_isnumber(L, stack)) {
		tx_gpio_id = (uint8_t)luaL_checkinteger( L, stack);
	    if (!platform_gpio_exists(tx_gpio_id) || tx_gpio_id == 0) {
	    	return luaL_error(L, "SoftUART tx GPIO not valid");
	    }
		stack++;
	} else {
		tx_gpio_id = 0xFF;
		stack++;
	}
	if (lua_isnumber(L, stack)) {
		rx_gpio_id = (uint8_t)luaL_checkinteger( L, stack);
	    if (!platform_gpio_exists(rx_gpio_id) || rx_gpio_id == 0) {
	    	return luaL_error(L, "SoftUART rx GPIO not valid");
	    }
	    if (softuart_gpio_instances[rx_gpio_id] != NULL) {
	    	return luaL_error( L, "SoftUART rx already configured on the pin.");
	    }
	} else {
		rx_gpio_id = 0xFF;
	}

	suart = (softuart_userdata*)lua_newuserdata(L, sizeof(softuart_userdata));
	suart->softuart = malloc(sizeof(softuart_t));
	if (!suart->softuart) {
		free(suart->softuart);
		suart->softuart = NULL;
		return luaL_error(L, "Not enough memory");
	}
	suart->softuart->pin_rx = rx_gpio_id;
	suart->softuart->pin_tx = tx_gpio_id;
	suart->softuart->need_len = RX_BUFF_SIZE;
	suart->softuart->armed = 0;
	//set bit time
    suart->softuart->bit_time = system_get_cpu_freq() * 1000000 / baudrate;

    // Set metatable
	luaL_getmetatable(L, "softuart.port");
	lua_setmetatable(L, -2);
	// Init SoftUART
	softuart_init(suart->softuart);
	return 1;
}

static void softuart_rx_callback(task_param_t arg)
{
	softuart_t *softuart = (softuart_t*)arg; //Receive pointer from ISR
	lua_State *L = lua_getstate();
	lua_rawgeti(L, LUA_REGISTRYINDEX, softuart_rx_cb_ref[softuart->pin_rx]);
	// Copy volatile data to static buffer
	for (int i = 0; i < softuart->buffer.length; i++) {
		softuart_rx_buffer[i] = softuart->buffer.receive_buffer[i];
	}
	lua_pushlstring(L, softuart_rx_buffer, softuart->buffer.length);
	softuart->buffer.length = 0;
	softuart->armed = 1;
	lua_call(L, 1, 0);
}

// Arguments: event name, minimum buffer filled to run callback, callback function
static int softuart_on(lua_State *L)
{
	NODE_DBG("SoftUART on called\n");
	softuart_userdata *suart = NULL;
	size_t name_len, arg_len;
	uint8_t stack = 1;

	suart = (softuart_userdata *)luaL_checkudata(L, 1, "softuart.port");
	luaL_argcheck(L, suart, stack, "softuart.port expected");
	if (suart == NULL) {
		NODE_DBG("Userdata is nil\n");
		return 0;
	}
	stack++;

	const char *method = luaL_checklstring(L, stack, &name_len);
	if (method == NULL)
		return luaL_error(L, "Wrong argument type");
	stack++;

	if (lua_type(L, stack) == LUA_TNUMBER) {
		suart->softuart->need_len = (uint16_t)luaL_checkinteger( L, stack );
		stack++;
		suart->softuart->end_char = 0;
		if (suart->softuart->need_len > SOFTUART_MAX_RX_BUFF) {
			suart->softuart->need_len = 0;
			return luaL_error(L, "Argument bigger than SoftUART buffer");
		}
		suart->softuart->armed = 1;
	} else if (lua_isstring(L, stack)) {
		const char *end = luaL_checklstring(L , stack,  &arg_len);
		stack++;
		if ( arg_len != 1) {
			return luaL_error(L, "Wrong end char length");
		}
		suart->softuart->end_char = end[0];
		suart->softuart->need_len = 0;
		suart->softuart->armed = 1;
	} else {
		return luaL_error(L, "Wrong argument type");
	}


	if (lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION) {
		lua_pushvalue(L, stack); // Copy to top of the stack
	} else {
		lua_pushnil(L);
	}

    if (name_len == 4 && strcmp(method, "data") == 0) {

    	if(suart->softuart->pin_rx == 0xFF) {
    		return luaL_error(L, "Rx pin was not declared");
    	}

    	if (softuart_rx_cb_ref[suart->softuart->pin_rx] != LUA_NOREF) {
    		luaL_unref(L, LUA_REGISTRYINDEX, softuart_rx_cb_ref[suart->softuart->pin_rx]);
    		softuart_rx_cb_ref[suart->softuart->pin_rx] = LUA_NOREF;
    	}
    	if (! lua_isnil(L, -1)) {
    		softuart_rx_cb_ref[suart->softuart->pin_rx] = luaL_ref(L, LUA_REGISTRYINDEX);
    	} else {
    		lua_pop(L, 1);
    	}
    } else {
    	lua_pop(L, 1);
    	return luaL_error(L, "Method not supported");
    }
    return 0;
}

static int softuart_write(lua_State *L)
{
	NODE_DBG("SoftUART write called\n");
	softuart_userdata *suart = NULL;
	uint8_t stack = 1;
	size_t str_len;
	suart = (softuart_userdata *)luaL_checkudata(L, 1, "softuart.port");
	luaL_argcheck(L, suart, stack, "softuart.port expected");
	if (suart == NULL) {
		NODE_DBG("Userdata is nil\n");
		return 0;
	}
	stack++;
	if(suart->softuart->pin_tx == 0xFF) {
		return luaL_error(L, "Tx pin was not declared");
	}
	if (lua_type(L, stack) == LUA_TNUMBER) {
		// Send byte
		uint32_t byte = (uint32_t)luaL_checkinteger( L, stack );
		if (byte > 255) {
			return luaL_error(L, "Integer too large for a byte");
		}
		softuart_putchar(suart->softuart, (char)byte);
	} else if (lua_isstring(L, stack)) {
		// Send string
		const char *string = luaL_checklstring(L , stack,  &str_len);
		for (size_t i = 0; i < str_len; i++) {
			softuart_putchar(suart->softuart, string[i]);
		}
	} else {
		return luaL_error(L, "Wrong argument type");
    }
	return 0;
}

static int softuart_gcdelete(lua_State *L)
{
	NODE_DBG("SoftUART GC called\n");
	softuart_userdata *suart = NULL;
	suart = (softuart_userdata *)luaL_checkudata(L, 1, "softuart.port");
	luaL_argcheck(L, suart, 1, "softuart.port expected");
	if (suart == NULL) {
		NODE_DBG("Userdata is nil\n");
		return 0;
	}
	softuart_gpio_instances[suart->softuart->pin_rx] = NULL;
	luaL_unref(L, LUA_REGISTRYINDEX, softuart_rx_cb_ref[suart->softuart->pin_rx]);
	softuart_rx_cb_ref[suart->softuart->pin_rx] = LUA_NOREF;
	free(suart->softuart);
	return 0;
}

// Port function map
LROT_BEGIN(softuart_port)
	LROT_FUNCENTRY( on, softuart_on)
	LROT_FUNCENTRY( write, softuart_write)
	LROT_TABENTRY( __index, softuart_port)
	LROT_FUNCENTRY( __gc, softuart_gcdelete)
LROT_END(ads1115, softuart_port, LROT_MASK_GC_INDEX)

// Module function map
LROT_BEGIN(softuart)
	LROT_FUNCENTRY( setup, softuart_setup)
	LROT_TABENTRY(__metatable, softuart_port)
LROT_END(softuart, NULL, 0 )

static int luaopen_softuart(lua_State *L)
{
	for(int i = 0; i < SOFTUART_GPIO_COUNT; i++) {
		softuart_rx_cb_ref[i] = LUA_NOREF;
	}
	uart_recieve_task = task_get_id((task_callback_t) softuart_rx_callback);
	luaL_rometatable(L, "softuart.port", (void *)softuart_port_map);
	return 0;
}

NODEMCU_MODULE(SOFTUART, "softuart", softuart, luaopen_softuart);
