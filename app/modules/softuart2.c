/*
 * Filename: softuart2.c
 * Author: sjp27
 * Date: 01/03/2021
 * Description: softuart2 module.
 */

#include "osapi.h"
#include "module.h"
#include "lauxlib.h"
#include "task/task.h"
#include "driver/softuart2.h"

// Callback reference
static int softuart2_rx_cb_ref;

// Task for receiving data
static task_handle_t uart_recieve_task = 0;

static void softuart2_rx_callback(softuart2_t *z_moduleData)
{
    task_post_medium(uart_recieve_task, (task_param_t)z_moduleData);
}

static void softuart2_process_rx(task_param_t z_arg)
{
    int i;
    char rx_buffer[SOFTUART2_MAX_BUFF];
    
	softuart2_t *softuart2 = (softuart2_t*)z_arg;

    ets_intr_lock();
	
    for(i = 0; i < SOFTUART2_MAX_BUFF; i++)
    {
	    uint8_t rx_byte;
	    
	    if(circular_buffer_pop(&softuart2->rx_buffer, &rx_byte) == 0)
        {
	        rx_buffer[i] = rx_byte;
        }
	    else
        {
            break;
        }
    }

    ets_intr_unlock();

    if(i > 0)
    {
        lua_State* L = lua_getstate();
        lua_rawgeti(L, LUA_REGISTRYINDEX, softuart2_rx_cb_ref);
        lua_pushlstring(L, rx_buffer, i);
        luaL_pcallx(L, 1, 0);
    }
}

static int softuart2_setup(lua_State *L)
{
	uint32_t baudrate;
	uint8_t tx_gpio_id, rx_gpio_id, clk_gpio_id;
    softuart2_t *softuart2;

	NODE_DBG("[SoftUART2]: setup called\n");
	baudrate = (uint32_t)luaL_checkinteger(L, 1);
	luaL_argcheck(L, (baudrate > 0 && baudrate <= 4800), 1, "Invalid baud rate");
	
    if (lua_isnoneornil(L, 2))
    {
        tx_gpio_id = 0xFF;
    }
    else
    {
        tx_gpio_id = (uint8_t)luaL_checkinteger(L, 2);
        luaL_argcheck(L, (platform_gpio_exists(tx_gpio_id) && tx_gpio_id != 0), 2, "Invalid SoftUART2 tx GPIO");
    }
    
    if (lua_isnoneornil(L, 3))
    {
        rx_gpio_id = 0xFF;
    }
    else
    {
        rx_gpio_id = (uint8_t)luaL_checkinteger(L, 3);
        luaL_argcheck(L, (platform_gpio_exists(rx_gpio_id) && rx_gpio_id != 0), 3, "Invalid SoftUART2 rx GPIO");
    }

    if (lua_isnoneornil(L, 4))
    {
        clk_gpio_id = 0xFF;
    }
    else
    {
        clk_gpio_id = (uint8_t)luaL_checkinteger(L, 4);
        luaL_argcheck(L, (platform_gpio_exists(clk_gpio_id) && clk_gpio_id != 0), 4, "Invalid SoftUART2 clk GPIO");
    }

    // SoftUART2 object without transmit and receive part would be useless
    if ((tx_gpio_id == 0xFF) && (rx_gpio_id == 0xFF))
    {
        return luaL_error(L, "Not enough arguments");
    }
	
	softuart2 = softuart2_drv_init();

	if(softuart2 == NULL)
    {
        return luaL_error(L, "Failed to initialise softuart2");
    }

    softuart2->baudrate = baudrate;
	softuart2->pin_rx = rx_gpio_id;
	softuart2->pin_tx = tx_gpio_id;
	softuart2->pin_clk = clk_gpio_id;
	softuart2->need_len = SOFTUART2_MAX_BUFF;
	softuart2->rx_callback = softuart2_rx_callback;

    if (!softuart2_drv_start())
    {
        return luaL_error(L, "Failed to start softuart2");
    }

    // Set metatable
	luaL_getmetatable(L, "softuart2.port");
	lua_setmetatable(L, -2);
    return 1;
}

static int softuart2_on(lua_State *L)
{
    softuart2_t *softuart2 = softuart2_drv_get_data();
	NODE_DBG("[SoftUART2] on: called\n");
	size_t name_len, arg_len;

	const char *method = luaL_checklstring(L, 2, &name_len);
	luaL_argcheck(L, lua_isfunction(L, 4), -1, "No callback function specified");
	luaL_argcheck(L, (name_len == 4 && strcmp(method, "data") == 0), 2, "Method not supported");
    luaL_argcheck(L, softuart2->pin_rx != 0xFF, 1, "Rx pin was not declared");

	if (lua_isnumber(L, 3))
	{
		luaL_argcheck(L, luaL_checkinteger(L, 3) < SOFTUART2_MAX_BUFF, 2, "Argument bigger than SoftUART2 buffer");
		softuart2->end_char = 0;
		softuart2->need_len = (sint16_t) luaL_checkinteger(L, 3);
	}
	else if (lua_isstring(L, 3))
	{
		const char *end = luaL_checklstring(L , 3,  &arg_len);
		luaL_argcheck(L, arg_len == 1, 3, "Wrong end char length");
		softuart2->end_char = end[0];
		softuart2->need_len = 0;
	}
	else
	{
		return luaL_error(L, "Wrong argument type");
	}

    lua_settop(L, 4); // Move to the top of the stack
    
    // Register callback or reregister new one
    luaL_reref(L, LUA_REGISTRYINDEX, &softuart2_rx_cb_ref);
    return 0;
}

static int softuart2_write(lua_State *L)
{
    softuart2_t *softuart2 = softuart2_drv_get_data();
	luaL_argcheck(L, softuart2->pin_tx != 0xFF, 1, "Tx pin was not declared");

	if (lua_isnumber(L, 2))
	{
		// Send byte
		uint32_t byte = (uint32_t)luaL_checkinteger(L, 2);
		luaL_argcheck(L, byte < 256, 2, "Integer too large for a byte");
        ets_intr_lock();
        circular_buffer_push(&softuart2->tx_buffer, (char)byte);
        ets_intr_unlock();
	}
	else if (lua_isstring(L, 2))
	{
		// Send string
    	size_t str_len;
    	
		const char *string = luaL_checklstring(L, 2, &str_len);
		for (size_t i = 0; i < str_len; i++)
		{
			ets_intr_lock();
			circular_buffer_push(&softuart2->tx_buffer, string[i]);
        	ets_intr_unlock();
		}
	}
	else
	{
		return luaL_error(L, "Wrong argument type");
    }
	return 0;
}

static int softuart2_gcdelete(lua_State *L)
{
	NODE_DBG("SoftUART2 GC called\n");
	softuart2_drv_delete();
	luaL_unref2(L, LUA_REGISTRYINDEX, softuart2_rx_cb_ref);
	return 0;
}

// Port function map
LROT_BEGIN(softuart2_port, NULL, LROT_MASK_GC_INDEX)
	LROT_FUNCENTRY( __gc, softuart2_gcdelete)
	LROT_TABENTRY( __index, softuart2_port)
	LROT_FUNCENTRY( on, softuart2_on)
	LROT_FUNCENTRY( write, softuart2_write)
LROT_END(softuart2_port, NULL, LROT_MASK_GC_INDEX)

// Module function map
LROT_BEGIN(softuart2, LROT_TABLEREF(softuart2_port), 0)
	LROT_FUNCENTRY( setup, softuart2_setup)
LROT_END(softuart2, LROT_TABLEREF(softuart2_port), 0)

static int luaopen_softuart2(lua_State *L)
{
	softuart2_rx_cb_ref = LUA_NOREF;
	uart_recieve_task = task_get_id((task_callback_t) softuart2_process_rx);
	luaL_rometatable(L, "softuart2.port", LROT_TABLEREF(softuart2_port));
	return 0;
}

NODEMCU_MODULE(SOFTUART2, "softuart2", softuart2, luaopen_softuart2);
