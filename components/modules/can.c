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

#include "task/task.h"

CAN_device_t CAN_cfg = {
  .speed = CAN_SPEED_1000KBPS,    // CAN Node baudrade
  .tx_pin_id = GPIO_NUM_5,    // CAN TX pin
  .rx_pin_id = GPIO_NUM_4,    // CAN RX pin
  .rx_queue = NULL,          // FreeRTOS queue for RX frames
  .code = 0,
  .mask = 0xffffffff,
  .dual_filter = false
};

static int can_on_received = LUA_NOREF;

static xTaskHandle  xCanTaskHandle = NULL;

static void task_CAN( void *pvParameters ){
  (void)pvParameters;
  
  lua_State *L = lua_getstate();

  //frame buffer
  CAN_frame_t __RX_frame;

  //create CAN RX Queue
  CAN_cfg.rx_queue = xQueueCreate(10, sizeof(CAN_frame_t));

  //start CAN Module
  CAN_init();

  for (;;){
    //receive next CAN frame from queue
    if( xQueueReceive( CAN_cfg.rx_queue, &__RX_frame, 3 * portTICK_PERIOD_MS ) == pdTRUE ){
      lua_rawgeti(L, LUA_REGISTRYINDEX, can_on_received);
      lua_pushinteger(L, __RX_frame.MsgID);
      lua_pushlstring(L, (char *)__RX_frame.data.u8, __RX_frame.DLC); 
      lua_call(L, 2, 0);
    }
  }
}

// Lua: setup( {} )
static int can_setup( lua_State *L )
{
  luaL_checkanytable (L, 1);

  luaL_checkanyfunction (L, 2);
  lua_settop (L, 2);
  if(can_on_received != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, can_on_received);
  can_on_received = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_getfield (L, 1, "speed");
  CAN_cfg.speed = luaL_optint(L, -1, 1000);
  lua_getfield (L, 1, "tx");
  CAN_cfg.tx_pin_id = luaL_optint(L, -1, GPIO_NUM_5);
  lua_getfield (L, 1, "rx");
  CAN_cfg.rx_pin_id = luaL_optint(L, -1, GPIO_NUM_4);
  lua_getfield (L, 1, "dual_filter");
  CAN_cfg.dual_filter = lua_toboolean(L, -1);
  lua_getfield (L, 1, "code");
  CAN_cfg.code = luaL_optint(L, -1, 0);
  lua_getfield (L, 1, "mask");
  CAN_cfg.mask = luaL_optint(L, -1, 0xffffffff);
  return 0;
}

static int can_start( lua_State *L )
{
  if(xCanTaskHandle != NULL)
    luaL_error( L, "can started" );
  xTaskCreate(&task_CAN, "CAN", 2048, NULL, ESP_TASK_MAIN_PRIO + 1, &xCanTaskHandle);
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
  uint32_t msg_id = luaL_checkinteger( L, 1 );
  size_t len;
  const char *data = luaL_checklstring( L, 2, &len );
  uint8_t i;
  CAN_frame_t frame;
  
  if(len > 8)
    luaL_error( L, "can not send more than 8 bytes" );
  
  frame.MsgID = msg_id;
  frame.DLC = len;
  
  for(i = 0; i < len; i++)
    frame.data.u8[i] = data[i];

  CAN_write_frame(&frame);
  return 0;
}

// Module function map
static const LUA_REG_TYPE can_map[] =
{
  { LSTRKEY( "setup" ),       LFUNCVAL( can_setup ) },
  { LSTRKEY( "start" ),       LFUNCVAL( can_start ) },
  { LSTRKEY( "stop" ),       LFUNCVAL( can_stop ) },
  { LSTRKEY( "send" ),        LFUNCVAL( can_send ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(CAN, "can", can_map, NULL);
