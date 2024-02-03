/*
 * Module for interfacing with cheap matrix keyboards like telephone keypads
 *
 * The idea is to have pullups on all the rows, and drive the columns low.
 * WHen a key is pressed, one of the rows will go low and trigger an interrupt. Disable
 * all the row interrupts.
 * Then we disable all the columns and then drive each column low in turn. Hopefully
 * one of the rows will go low. This is a keypress. We only report the first keypress found.
 * we start a timer to handle debounce.
 * On timer expiry, see if any key is pressed, if so, just wait agin (maybe should use interrupts)
 * If no key is pressed, run timer again. On timer expiry, re-enable interrupts.
 *
 * Philip Gladstone, N1DQ
 */

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "task/task.h"
#include "esp_timer.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


#include <stdio.h>

#include "driver/gpio.h"

#define M_DEBUG printf

#define MATRIX_PRESS_INDEX	0
#define MATRIX_RELEASE_INDEX	1

#define MASK(x) (1 << MATRIX_##x##_INDEX)

#define MATRIX_ALL		0x3

#define CALLBACK_COUNT	2
#define QUEUE_SIZE 8

typedef struct {
  int32_t character;    // 1 + character for press, -1 - character for release
  uint32_t time_us;
} matrix_event_t;

typedef struct {
  uint8_t column_count;
  uint8_t row_count;
  uint8_t *columns;
  uint8_t *rows;
  bool waiting_for_release;
  bool open;
  int character_ref;
  int callback[CALLBACK_COUNT];
  esp_timer_handle_t timer_handle;
  int8_t task_queued;
  uint32_t read_offset;   // Accessed by task
  uint32_t write_offset;  // Accessed by ISR
  uint8_t last_character;
  matrix_event_t queue[QUEUE_SIZE];
  void *callback_arg;
} DATA;

static task_handle_t tasknumber;
static void lmatrix_timer_done(void *param);
//
//  Queue is empty if read == write.
//  However, we always want to keep the previous value
//  so writing is only allowed if write - read < QUEUE_SIZE - 1

#define GET_LAST_STATUS(d) (d->queue[(d->write_offset - 1) & (QUEUE_SIZE - 1)])
#define GET_PREV_STATUS(d) (d->queue[(d->write_offset - 2) & (QUEUE_SIZE - 1)])
#define HAS_QUEUED_DATA(d) (d->read_offset < d->write_offset)
#define HAS_QUEUE_SPACE(d) (d->read_offset + QUEUE_SIZE - 1 > d->write_offset)

#define REPLACE_IT(d, x)                            \
  (d->queue[(d->write_offset - 1) & (QUEUE_SIZE - 1)] = \
       (matrix_event_t){(x), esp_timer_get_time()})
#define QUEUE_IT(d, x)                            \
  (d->queue[(d->write_offset++) & (QUEUE_SIZE - 1)] = \
       (matrix_event_t){(x), esp_timer_get_time()})

#define GET_READ_STATUS(d) (d->queue[d->read_offset & (QUEUE_SIZE - 1)])
#define ADVANCE_IF_POSSIBLE(d)            \
  if (d->read_offset < d->write_offset) { \
    d->read_offset++;                     \
  }

static void set_gpio_mode_input(int pin, gpio_int_type_t intr) {
  gpio_config_t config = {.pin_bit_mask = 1LL << pin,
                          .mode = GPIO_MODE_INPUT,
                          .pull_up_en = GPIO_PULLUP_ENABLE,
                          .pull_down_en = GPIO_PULLDOWN_DISABLE,
                          .intr_type = intr};

  gpio_config(&config);
}

static void set_gpio_mode_output(int pin) {
  gpio_config_t config = {.pin_bit_mask = 1LL << pin,
                          .mode = GPIO_MODE_OUTPUT_OD,
                          .pull_up_en = GPIO_PULLUP_DISABLE,
                          .pull_down_en = GPIO_PULLDOWN_DISABLE
                        };

  gpio_config(&config);
}

static void set_columns(DATA *d, int level) {
  for (int i = 0; i < d->column_count; i++) {
    gpio_set_level(d->columns[i], level);
  }
}

static void initialize_pins(DATA *d) {
  for (int i = 0; i < d->column_count; i++) {
    set_gpio_mode_output(d->columns[i]);
  }

  set_columns(d, 0);

  for (int i = 0; i < d->row_count; i++) {
    set_gpio_mode_input(d->rows[i], d->waiting_for_release ? GPIO_INTR_POSEDGE
                                                           : GPIO_INTR_NEGEDGE);
  }
}

static void disable_row_interrupts(DATA *d) {
  for (int i = 0; i < d->row_count; i++) {
    gpio_set_intr_type(d->rows[i], GPIO_INTR_DISABLE);
  }
}

// Just takes the channel number. Cleans up the resources used.
int matrix_close(DATA *d) {
  if (!d) {
    return 0;
  }

  disable_row_interrupts(d);

  for (int i = 0; i < d->row_count; i++) {
    gpio_isr_handler_remove(d->rows[i]);
  }

  for (int i = 0; i < d->column_count; i++) {
    set_gpio_mode_input(d->columns[i], GPIO_INTR_DISABLE);
  }

  return 0;
}

// Character returned is 0 .. max if pressed. -1 if not.
static int matrix_get_character(DATA *d, bool trace)
{
  set_columns(d, 1);
  disable_row_interrupts(d);

  int character = -1;

  // We are either waiting for a negative edge (keypress) or a positive edge
  // (keyrelease)

  //M_DEBUG("matrix_get_character called\n");

  for (int i = 0; i < d->column_count && character < 0; i++) {
    gpio_set_level(d->columns[i], 0);

    for (int j = 0; j < d->row_count && character < 0; j++) {
      if (gpio_get_level(d->rows[j]) == 0) {
        if (trace) {
          M_DEBUG("Found keypress at %d %d\n", i, j);
        }
        // We found a keypress
        character = j * d->column_count + i;
      }
    }

    gpio_set_level(d->columns[i], 1);
  }

  //M_DEBUG("returning %d\n", character);

  return character;
}

static void matrix_queue_character(DATA *d, int character)
{
  // If character is >= 0 then we have found the character -- so send it.
  // M_DEBUG("Skipping queuing\n");

  if (d->waiting_for_release == (character < 0)) {
    if (character >= 0) {
      character++;
      d->last_character = character;
    } else {
      character = -d->last_character;
    }

    if (HAS_QUEUE_SPACE(d)) {
      QUEUE_IT(d, character);
      if (!d->task_queued) {
        if (task_post_medium(tasknumber, (task_param_t)d)) {
          d->task_queued = 1;
        }
      }
    }
  }
}

static void matrix_interrupt(void *arg) {
  // This function runs with high priority
  DATA *d = (DATA *)arg;

  int character = matrix_get_character(d, false);

  matrix_queue_character(d, character);

  d->waiting_for_release = character >= 0;
  esp_timer_start_once(d->timer_handle, 5000);
}

bool matrix_has_queued_event(DATA *d) {
  if (!d) {
    return false;
  }

  return HAS_QUEUED_DATA(d);
}

// Get the oldest event in the queue and remove it (if possible)
static bool matrix_getevent(DATA *d, matrix_event_t *resultp) {
  matrix_event_t result = {0};

  if (!d) {
    return false;
  }

  bool status = false;

  if (HAS_QUEUED_DATA(d)) {
    result = GET_READ_STATUS(d);
    d->read_offset++;
    status = true;
  } else {
    result = GET_LAST_STATUS(d);
  }

  *resultp = result;

  return status;
}

static void callback_free_one(lua_State *L, int *cb_ptr)
{
  if (*cb_ptr != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, *cb_ptr);
    *cb_ptr = LUA_NOREF;
  }
}

static void callback_free(lua_State* L, DATA *d, int mask)
{
  if (d) {
    int i;
    for (i = 0; i < CALLBACK_COUNT; i++) {
      if (mask & (1 << i)) {
        callback_free_one(L, &d->callback[i]);
      }
    }
  }
}

static int callback_setOne(lua_State* L, int *cb_ptr, int arg_number)
{
  if (lua_isfunction(L, arg_number)) {
    lua_pushvalue(L, arg_number);  // copy argument (func) to the top of stack
    callback_free_one(L, cb_ptr);
    *cb_ptr = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
  }

  return -1;
}

static int callback_set(lua_State* L, DATA *d, int mask, int arg_number)
{
  int result = 0;

  int i;
  for (i = 0; i < CALLBACK_COUNT; i++) {
    if (mask & (1 << i)) {
      result |= callback_setOne(L, &d->callback[i], arg_number);
    }
  }

  return result;
}

static void callback_callOne(lua_State* L, int cb, int mask, int arg, uint32_t time)
{
  if (cb != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, cb);

    lua_pushinteger(L, mask);
    lua_pushvalue(L, arg - 2);
    lua_pushinteger(L, time);

    luaL_pcallx(L, 3, 0);
  }
}

static void callback_call(lua_State* L, DATA *d, int cbnum, int key, uint32_t time)
{
  if (d) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, d->character_ref);
    lua_rawgeti(L, -1, key);
    callback_callOne(L, d->callback[cbnum], 1 << cbnum, -1, time);
    lua_pop(L, 2);
  }
}

static void getpins(lua_State *L, int argno, int count, uint8_t *dest)
{
  for (int i = 1; i <= count; i++) {
    lua_rawgeti(L, argno, i);
    *dest++ = lua_tonumber(L, -1);
    lua_pop(L, 1);
  }
}

// Lua: setup({cols}, {rows}, {characters})
static int lmatrix_setup( lua_State* L )
{
  // Get the sizes of the first two tables
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checktype(L, 2, LUA_TTABLE);
  luaL_checktype(L, 3, LUA_TTABLE);

  size_t columns = lua_rawlen(L, 1);
  size_t rows = lua_rawlen(L, 2);

  if (columns > 255 || rows > 255) {
    return luaL_error(L, "Too many rows or columns");
  }

  DATA *d = (DATA *)lua_newuserdata(L, sizeof(DATA) + rows + columns);
  if (!d) return luaL_error(L, "not enough memory");
  memset(d, 0, sizeof(*d) + rows + columns);
  luaL_getmetatable(L, "matrix.keyboard");
  lua_setmetatable(L, -2);

  d->columns = (uint8_t *) (d + 1);
  d->rows = d->columns + columns;
  d->column_count = columns;
  d->row_count = rows;

  esp_timer_create_args_t timer_args = {
    .callback = lmatrix_timer_done,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "matrix_timer",
    .arg = d
  };

  esp_timer_create(&timer_args, &d->timer_handle);

  for (int i = 0; i < CALLBACK_COUNT; i++) {
    d->callback[i] = LUA_NOREF;
  }
  getpins(L, 1, columns, d->columns);
  getpins(L, 2, rows, d->rows);
  lua_pushvalue(L, 3);
  d->character_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  d->open = true;

  for (int i = 0; i < d->row_count; i++) {
    gpio_isr_handler_add(d->rows[i], matrix_interrupt, d);
  }
  initialize_pins(d);

  return 1;
}

// Lua: close( )
static int lmatrix_close( lua_State* L )
{
  DATA *d = (DATA *)luaL_checkudata(L, 1, "matrix.keyboard");

  if (d->open) {
    callback_free(L, d, MATRIX_ALL);

    if (matrix_close( d )) {
      return luaL_error( L, "Unable to close switch." );
    }

    esp_timer_stop(d->timer_handle);
    esp_timer_delete(d->timer_handle);
    luaL_unref(L, LUA_REGISTRYINDEX, d->character_ref);

    d->open = false;
  }
  return 0;
}

// Lua: on( mask[, cb] )
static int lmatrix_on( lua_State* L )
{
  DATA *d = (DATA *)luaL_checkudata(L, 1, "matrix.keyboard");

  int mask = luaL_checkinteger(L, 2);

  if (lua_gettop(L) >= 3) {
    if (callback_set(L, d, mask, 3)) {
      return luaL_error( L, "Unable to set callback." );
    }
  } else {
    callback_free(L, d, mask);
  }

  return 0;
}

// Returns TRUE if there maybe/is more stuff to do
static bool lmatrix_dequeue_single(lua_State* L, DATA *d)
{
  bool something_pending = false;

  if (d) {
    matrix_event_t result;

    if (matrix_getevent(d, &result)) {
      int character = result.character;

      callback_call(L, d, character > 0 ? MATRIX_PRESS_INDEX : MATRIX_RELEASE_INDEX, character < 0 ? -character : character, result.time_us);

      d->task_queued = 0;
      something_pending = matrix_has_queued_event(d);
    }
  }

  return something_pending;
}

static void lmatrix_timer_done(void *param)
{
  DATA *d = (DATA *) param;

  // We need to see if the key is still pressed, and if so, enable rising edge interrupts

  int character = matrix_get_character(d, true);

  M_DEBUG("Timer fired with character %d (waiting for release %d)\n", character, d->waiting_for_release);

  matrix_queue_character(d, character);
  d->waiting_for_release = (character >= 0);

  M_DEBUG("Timer: Waiting for release is %d\n", d->waiting_for_release);

  for (int i = 0; i < d->row_count; i++) {
    gpio_set_intr_type(d->rows[i], d->waiting_for_release ? GPIO_INTR_POSEDGE
                                                          : GPIO_INTR_NEGEDGE);
  }
  set_columns(d, 0);
}

static void lmatrix_task(task_param_t param, task_prio_t prio)
{
  (void) prio;

  M_DEBUG("Task invoked\n");

  bool need_to_post = false;
  lua_State *L = lua_getstate();

  DATA *d = (DATA *) param;
  if (d) {
    if (lmatrix_dequeue_single(L, d)) {
      need_to_post = true;
    }
  }

  if (need_to_post) {
    // If there is pending stuff, queue another task
    task_post_medium(tasknumber, param);
  }

  M_DEBUG("Task done\n");
}


// Module function map
LROT_BEGIN(matrix, NULL, 0)
  LROT_FUNCENTRY( setup, lmatrix_setup )
  LROT_NUMENTRY( PRESS, MASK(PRESS) )
  LROT_NUMENTRY( RELEASE, MASK(RELEASE) )
  LROT_NUMENTRY( ALL, MATRIX_ALL )
LROT_END(matrix, NULL, 0)

// Module function map
LROT_BEGIN(matrix_keyboard, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY(__gc, lmatrix_close)
  LROT_TABENTRY(__index, matrix_keyboard)
  LROT_FUNCENTRY(on, lmatrix_on)
  LROT_FUNCENTRY(close, lmatrix_close)
LROT_END(matrix_keyboard, NULL, LROT_MASK_GC_INDEX)

static int matrix_open(lua_State *L) {
  luaL_rometatable(L, "matrix.keyboard",
                    LROT_TABLEREF(matrix_keyboard));  // create metatable
  tasknumber = task_get_id(lmatrix_task);
  return 0;
}

NODEMCU_MODULE(MATRIX, "matrix", matrix, matrix_open);
