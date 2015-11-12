#ifndef __USER_SENSOR_H__
#define __USER_SENSOR_H__

#include "user_config.h"
#include "driver/key.h"

#define SENSOR_KEY_NUM    1

#define SENSOR_KEY_IO_MUX     PERIPHS_IO_MUX_MTCK_U
#define SENSOR_KEY_IO_NUM     13
#define SENSOR_KEY_IO_FUNC    FUNC_GPIO13

#define SENSOR_WIFI_LED_IO_MUX     PERIPHS_IO_MUX_GPIO0_U
#define SENSOR_WIFI_LED_IO_NUM     0
#define SENSOR_WIFI_LED_IO_FUNC    FUNC_GPIO0

#define SENSOR_LINK_LED_IO_MUX     PERIPHS_IO_MUX_MTDI_U
#define SENSOR_LINK_LED_IO_NUM     12
#define SENSOR_LINK_LED_IO_FUNC    FUNC_GPIO12

#define SENSOR_UNUSED_LED_IO_MUX     PERIPHS_IO_MUX_MTDO_U
#define SENSOR_UNUSED_LED_IO_NUM     15
#define SENSOR_UNUSED_LED_IO_FUNC    FUNC_GPIO15

#if HUMITURE_SUB_DEVICE
bool user_mvh3004_read_th(uint8 *data);
void user_mvh3004_init(void);
#endif

void user_sensor_init(uint8 active);

#endif
