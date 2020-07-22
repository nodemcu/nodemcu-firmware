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

//TODO: Overflow flag as callback function + docs

// Additional buffer with just 2 frames to be able to copy from one and write to another
typedef struct {
	char isr_receive_buffer[2];
	uint8_t active_frame;
} softuart_isr_buffer_t;

typedef struct {
	char receive_buffer[SOFTUART_MAX_RX_BUFF];
	uint8_t buffer_first;
	uint8_t buffer_last;
	uint8_t bytes_count;
	uint8_t buffer_overflow;
} softuart_buffer_t;

typedef struct {
	volatile softuart_isr_buffer_t isr_buffer;
	uint16_t bit_time;
	uint16_t need_len; // Buffer length needed to run callback function
	char end_char; // Used to run callback if last char in buffer will be the same
	softuart_buffer_t buffer;
	uint8_t armed;
	uint8_t pin_rx;
	uint8_t pin_tx;
} softuart_t;

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

static uint32_t ICACHE_RAM_ATTR softuart_intr_handler(uint32_t ret_gpio_status)
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
		// Clear interrupt status on that pin
		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & (1 << pin_num[s->pin_rx]));
		ret_gpio_status &= ~(1 << pin_num[s->pin_rx]);
		if (softuart_rx_cb_ref[pin_num_inv[gpio_bit]] == LUA_NOREF) continue;
		if (!s->armed) continue;
		// Read frame and save it to buffer
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
					// If high, set MSB of byte to 1
					byte |= 0x80;
				}
				// Recalculate start time for next bit
				start_time += s->bit_time;
			}

			// Save byte to correct buffer slot
			if (s->isr_buffer.active_frame == 0) {
				s->isr_buffer.isr_receive_buffer[0] = byte;
				s->isr_buffer.active_frame = 1;
			} else {
				s->isr_buffer.isr_receive_buffer[1] = byte;
				s->isr_buffer.active_frame = 0;
			}
			// Send the data
			task_post_medium(uart_recieve_task, (task_param_t)s);
			// wait for stop bit
			while ((uint32_t)(asm_ccount() - start_time) < s->bit_time);
			// Break the loop after reading of the frame
			break;
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
		// Wait to transmit another bit
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

static int softuart_init(softuart_t *s)
{

	// Init tx pin
    if (s->pin_tx != 0xFF) {
        platform_gpio_mode(s->pin_tx, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_PULLUP);
        platform_gpio_write(s->pin_tx, PLATFORM_GPIO_HIGH);
    }

    // Init rx pin
    if (s->pin_rx != 0xFF) {
		platform_gpio_mode(s->pin_rx, PLATFORM_GPIO_INT, PLATFORM_GPIO_PULLUP);
		// Enable interrupt for pin on falling edge
		platform_gpio_intr_init(s->pin_rx, GPIO_PIN_INTR_NEGEDGE);
		softuart_gpio_instances[s->pin_rx] = s;
		// Preserve other rx gpio pins
		uint32_t mask = 0;
		for (uint8_t i = 0; i < SOFTUART_GPIO_COUNT; i++) {
			if (softuart_gpio_instances[i] != NULL) {
				mask = mask | (1 << pin_num[softuart_gpio_instances[i]->pin_rx]);
			}
		}
		return platform_gpio_register_intr_hook(mask, softuart_intr_handler);
    }
}


static int softuart_setup(lua_State *L)
{
	uint32_t baudrate;
	uint8_t tx_gpio_id, rx_gpio_id;
	softuart_t *softuart = NULL;

	NODE_DBG("[SoftUART]: setup called\n");
	baudrate = (uint32_t)luaL_checkinteger(L, 1); // Get Baudrate from
	luaL_argcheck(L, (baudrate > 0 && baudrate < 230400), 1, "Invalid baud rate");
	lua_remove(L, 1); // Remove baudrate argument from stack
	if (lua_gettop(L) == 2) { // 2 arguments: 1st can be nil
		if (lua_isnil(L, 1)) {
			tx_gpio_id = 0xFF;
		} else {
			tx_gpio_id = (uint8_t)luaL_checkinteger(L, 1);
			luaL_argcheck(L, (platform_gpio_exists(tx_gpio_id) && tx_gpio_id != 0)
				, 2, "Invalid SoftUART tx GPIO");
		}
		rx_gpio_id = (uint8_t)luaL_checkinteger(L, 2);
		luaL_argcheck(L, (platform_gpio_exists(rx_gpio_id) && rx_gpio_id != 0)
			, 3, "Invalid SoftUART rx GPIO");
		luaL_argcheck(L, softuart_gpio_instances[rx_gpio_id] == NULL
			, 3, "SoftUART rx already configured on the pin");

	} else if (lua_gettop(L) == 1) { // 1 argument: transmit part only
		rx_gpio_id = 0xFF;
		tx_gpio_id = (uint8_t)luaL_checkinteger(L, 1);
		luaL_argcheck(L, (platform_gpio_exists(tx_gpio_id) && tx_gpio_id != 0)
			, 2, "Invalid SoftUART tx GPIO");
	} else {
		// SoftUART object without receive and transmit part would be useless
		return luaL_error(L, "Not enough arguments");
	}

	softuart = (softuart_t*)lua_newuserdata(L, sizeof(softuart_t));
	softuart->pin_rx = rx_gpio_id;
	softuart->pin_tx = tx_gpio_id;
	softuart->need_len = RX_BUFF_SIZE;
	softuart->armed = 0;

	// Set ISR buffer
	softuart->isr_buffer.isr_receive_buffer[0] = 0;
	softuart->isr_buffer.isr_receive_buffer[1] = 0;
	softuart->isr_buffer.active_frame = 0;
	// Set buffer
	softuart->buffer.buffer_first = 0;
	softuart->buffer.buffer_last = 0;
	softuart->buffer.bytes_count = 0;
	softuart->buffer.buffer_overflow = 0;

	// Set bit time
    softuart->bit_time = (system_get_cpu_freq() * 1000000 ) / baudrate;

    // Set metatable
	luaL_getmetatable(L, "softuart.port");
	lua_setmetatable(L, -2);
	// Init SoftUART
	int result = softuart_init(softuart);
	if (result == 0) {
		luaL_error(L, "Couldn't register interrupt");
	}
	return 1;
}

static void softuart_rx_callback(softuart_t* softuart)
{
	softuart->armed = 0;
	lua_State *L = lua_getstate();
	lua_rawgeti(L, LUA_REGISTRYINDEX, softuart_rx_cb_ref[softuart->pin_rx]);

	// Clear overflow flag if needed
	if(softuart->buffer.bytes_count == SOFTUART_MAX_RX_BUFF) {
		softuart->buffer.buffer_overflow = 0;
	}
	// Copy data to static buffer
	uint8_t buffer_length = softuart->buffer.bytes_count;
	for (int i = 0; i < buffer_length; i++) {
		softuart_rx_buffer[i] = softuart->buffer.receive_buffer[softuart->buffer.buffer_first];
		softuart->buffer.buffer_first++;
		softuart->buffer.bytes_count--;
		if (softuart->buffer.buffer_first == SOFTUART_MAX_RX_BUFF) {
			softuart->buffer.buffer_first = 0;
		}
	}
	lua_pushlstring(L, softuart_rx_buffer, buffer_length);
	softuart->armed = 1;
	luaL_pcallx(L, 1, 0);
}

static void softuart_rx_isr_callback(task_param_t arg) {
	//Receive pointer from ISR
	softuart_t *s = (softuart_t*)arg;
	// Disarm while reading buffer
	s->armed = 0;
	char byte = (s->isr_buffer.active_frame == 0) ?
			s->isr_buffer.isr_receive_buffer[0] : s->isr_buffer.isr_receive_buffer[1];
	s->armed = 1;
	// If buffer full, set the overflow flag and return
	if (s->buffer.bytes_count == SOFTUART_MAX_RX_BUFF) {
		s->buffer.buffer_overflow = 1;
	} else if (s->buffer.bytes_count < SOFTUART_MAX_RX_BUFF) {
		s->buffer.receive_buffer[s->buffer.buffer_last] = byte;
		s->buffer.buffer_last++;
		s->buffer.bytes_count++;

		// Check for overflow after appending new byte
		if (s->buffer.bytes_count == SOFTUART_MAX_RX_BUFF) {
			s->buffer.buffer_overflow = 1;
		}
		// Roll over buffer index if necessary
		if (s->buffer.buffer_last == SOFTUART_MAX_RX_BUFF) {
			s->buffer.buffer_last = 0;
		}

		// Check for callback conditions
		if (((s->need_len != 0) && (s->buffer.bytes_count >= s->need_len))  || \
				((s->need_len == 0) && ((char)byte == s->end_char))) {
					softuart_rx_callback(s);
		}
	}
}

// Arguments: event name, minimum buffer filled to run callback, callback function
static int softuart_on(lua_State *L)
{
	NODE_DBG("[SoftUART] on: called\n");
	size_t name_len, arg_len;

	softuart_t *softuart = (softuart_t*)luaL_checkudata(L, 1, "softuart.port");
	const char *method = luaL_checklstring(L, 2, &name_len);

	if (lua_isnumber(L, 3)) {
		luaL_argcheck(L, luaL_checkinteger(L, 3) < SOFTUART_MAX_RX_BUFF,
				2, "Argument bigger than SoftUART buffer");
		softuart->end_char = 0;
		softuart->need_len = (uint16_t) luaL_checkinteger(L, 3);
		softuart->armed = 1;
	} else if (lua_isstring(L, 3)) {
		const char *end = luaL_checklstring(L , 3,  &arg_len);
		luaL_argcheck(L, arg_len == 1, 3, "Wrong end char length");
		softuart->end_char = end[0];
		softuart->need_len = 0;
		softuart->armed = 1;
	} else {
		return luaL_error(L, "Wrong argument type");
	}

	luaL_argcheck(L, lua_isfunction(L, 4), -1, "No callback function specified");
	luaL_argcheck(L, (name_len == 4 && strcmp(method, "data") == 0), 2, "Method not supported");
    luaL_argcheck(L, softuart->pin_rx != 0xFF, 1, "Rx pin was not declared");
    lua_settop(L, 4); // Move to the top of the stack
    if (softuart_rx_cb_ref[softuart->pin_rx] != LUA_NOREF) {
    	// Remove old callback and add new one
    	luaL_unref(L, LUA_REGISTRYINDEX, softuart_rx_cb_ref[softuart->pin_rx]);
    }
    // Register callback
    softuart_rx_cb_ref[softuart->pin_rx] = luaL_ref(L, LUA_REGISTRYINDEX);

    return 0;
}

static int softuart_write(lua_State *L)
{
	softuart_t *softuart = NULL;
	size_t str_len;
	softuart = (softuart_t*) luaL_checkudata(L, 1, "softuart.port");
	luaL_argcheck(L, softuart->pin_tx != 0xFF, 1, "Tx pin was not declared");

	if (lua_isnumber(L, 2)) {
		// Send byte
		uint32_t byte = (uint32_t)luaL_checkinteger(L, 2);
		luaL_argcheck(L, byte < 256, 2, "Integer too large for a byte");
		softuart_putchar(softuart, (char)byte);
	} else if (lua_isstring(L, 2)) {
		// Send string
		const char *string = luaL_checklstring(L, 2, &str_len);
		for (size_t i = 0; i < str_len; i++) {
			softuart_putchar(softuart, string[i]);
		}
	} else {
		return luaL_error(L, "Wrong argument type");
    }
	return 0;
}

static int softuart_gcdelete(lua_State *L)
{
	NODE_DBG("SoftUART GC called\n");
	softuart_t *softuart = NULL;
	softuart = (softuart_t*) luaL_checkudata(L, 1, "softuart.port");
	uint8_t last_instance = 1;
	for(uint8_t instance = 0; instance < SOFTUART_GPIO_COUNT; instance++)
		if (softuart_gpio_instances[instance] != NULL && instance != softuart->pin_rx)
			last_instance = 0;

	softuart_gpio_instances[softuart->pin_rx] = NULL;
	luaL_unref(L, LUA_REGISTRYINDEX, softuart_rx_cb_ref[softuart->pin_rx]);
	softuart_rx_cb_ref[softuart->pin_rx] = LUA_NOREF;
	// Try to unregister the interrupt hook if this was last or the only instance
	if (last_instance)
		platform_gpio_register_intr_hook(0, softuart_intr_handler);
	return 0;
}

// Port function map
LROT_BEGIN(softuart_port, NULL, LROT_MASK_GC_INDEX)
	LROT_FUNCENTRY( __gc, softuart_gcdelete)
	LROT_TABENTRY( __index, softuart_port)
	LROT_FUNCENTRY( on, softuart_on)
	LROT_FUNCENTRY( write, softuart_write)
LROT_END(softuart_port, NULL, LROT_MASK_GC_INDEX)

// Module function map
LROT_BEGIN(softuart, LROT_TABLEREF(softuart_port), 0)
	LROT_FUNCENTRY( setup, softuart_setup)
LROT_END(softuart, LROT_TABLEREF(softuart_port), 0)

static int luaopen_softuart(lua_State *L)
{
	for(int i = 0; i < SOFTUART_GPIO_COUNT; i++) {
		softuart_rx_cb_ref[i] = LUA_NOREF;
	}
	uart_recieve_task = task_get_id((task_callback_t) softuart_rx_isr_callback);
	luaL_rometatable(L, "softuart.port", LROT_TABLEREF(softuart_port));
	return 0;
}

NODEMCU_MODULE(SOFTUART, "softuart", softuart, luaopen_softuart);
