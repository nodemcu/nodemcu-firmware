// Module for interfacing with adc hardware

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "CAN.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_task.h"
#include "esp_log.h"

#include <string.h>

#include "task/task.h"

CAN_device_t CAN_cfg = {
  .speed = CAN_SPEED_1000KBPS,    // CAN Node baudrade
  .tx_pin_id = -1,    // CAN TX pin
  .rx_pin_id = -1,    // CAN RX pin
  .rx_queue = NULL,          // FreeRTOS queue for RX frames
  .code = 0,
  .mask = 0xffffffff,
  .dual_filter = false
};

static task_handle_t can_data_task_id;
static int can_on_received = LUA_NOREF;

static TaskHandle_t  xCanTaskHandle = NULL;

// LUA
static void can_data_task( task_param_t param, task_prio_t prio ) {
  CAN_frame_t *frame = (CAN_frame_t *)param;

  if(can_on_received == LUA_NOREF) {
    free( frame );
    return;
  }
  lua_State *L = lua_getstate();

  lua_rawgeti(L, LUA_REGISTRYINDEX, can_on_received);
  lua_pushinteger(L, frame->Extended? 1 : 0);
  lua_pushinteger(L, frame->MsgID);
  lua_pushlstring(L, (char *)frame->data.u8, frame->DLC); 
  free( frame );
  luaL_pcallx(L, 3, 0);
}

// RTOS
static void task_CAN( void *pvParameters ){
  (void)pvParameters;
  
  //frame buffer
  CAN_frame_t frame;

  //create CAN RX Queue
  CAN_cfg.rx_queue = xQueueCreate(10, sizeof(CAN_frame_t));

  //start CAN Module
  CAN_init();

  for (;;){
    //receive next CAN frame from queue
    if( xQueueReceive( CAN_cfg.rx_queue, &frame, 3 * portTICK_PERIOD_MS ) == pdTRUE ){
	  CAN_frame_t *postFrame = (CAN_frame_t *)malloc( sizeof( CAN_frame_t ) );
	  memcpy(postFrame, &frame, sizeof( CAN_frame_t ));
      task_post_medium( can_data_task_id, (task_param_t)postFrame );
    }
  }
}

// Lua: setup( {}, callback )
static int can_setup( lua_State *L )
{
  if(xCanTaskHandle != NULL)
    luaL_error( L, "Stop CAN before setup" );
  luaL_checktable (L, 1);

  luaL_checkfunction (L, 2);
  lua_settop (L, 2);
  if(can_on_received != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, can_on_received);
  can_on_received = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_getfield (L, 1, "speed");
  CAN_cfg.speed = luaL_checkint(L, -1);
  lua_getfield (L, 1, "tx");
  CAN_cfg.tx_pin_id = luaL_checkint(L, -1);
  lua_getfield (L, 1, "rx");
  CAN_cfg.rx_pin_id = luaL_checkint(L, -1);
  lua_getfield (L, 1, "dual_filter");
  CAN_cfg.dual_filter = lua_toboolean(L, 0);
  lua_getfield (L, 1, "code");
  CAN_cfg.code = (uint32_t)luaL_optnumber(L, -1, 0);
  lua_getfield (L, 1, "mask");
  CAN_cfg.mask = (uint32_t)luaL_optnumber(L, -1, 0x0ffffffff);
  return 0;
}

static int can_start( lua_State *L )
{
  if(xCanTaskHandle != NULL)
    luaL_error( L, "CAN started" );
  xTaskCreate(task_CAN, "CAN", 2048, NULL, ESP_TASK_MAIN_PRIO + 1, &xCanTaskHandle);
  return 0;
}

static int can_stop( lua_State *L )
{
  if(xCanTaskHandle) {
    vTaskDelete(xCanTaskHandle);
    xCanTaskHandle = NULL;
  }
  CAN_stop();
  if(CAN_cfg.rx_queue) {
    vQueueDelete( CAN_cfg.rx_queue );
    CAN_cfg.rx_queue = NULL;
  }
  return 0;
}

static int can_send( lua_State *L )
{
  uint32_t format = (uint8_t)luaL_checkinteger( L, 1 );
  uint32_t msg_id = luaL_checkinteger( L, 2 );
  size_t len;
  const char *data = luaL_checklstring( L, 3, &len );
  uint8_t i;
  CAN_frame_t frame;
  
  if(xCanTaskHandle == NULL)
    luaL_error( L, "CAN not started" );
  
  if(len > 8)
    luaL_error( L, "CAN can not send more than 8 bytes" );
  
  frame.Extended = format? 1 : 0;
  frame.MsgID = msg_id;
  frame.DLC = len;
  
  for(i = 0; i < len; i++)
    frame.data.u8[i] = data[i];

  CAN_write_frame(&frame);
  return 0;
}

// Module function map
LROT_BEGIN(can, NULL, 0)
  LROT_FUNCENTRY( setup,          can_setup )
  LROT_FUNCENTRY( start,          can_start )
  LROT_FUNCENTRY( stop,           can_stop )
  LROT_FUNCENTRY( send,           can_send )
  LROT_NUMENTRY ( STANDARD_FRAME, 0 )
  LROT_NUMENTRY ( EXTENDED_FRAME, 1 )
LROT_END(can, NULL, 0)

int luaopen_can( lua_State *L ) {
  can_data_task_id = task_get_id( can_data_task ); // reset CAN after sw reset
  CAN_stop();
  return 0;
}

NODEMCU_MODULE(CAN, "can", can, luaopen_can);
