/**
 * @section License
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017, Thomas Barth, barth-dev.de
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __DRIVERS_CAN_CFG_H__
#define __DRIVERS_CAN_CFG_H__

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"


/** \brief CAN Node Bus speed */
typedef enum  {
	CAN_SPEED_100KBPS=100, 				/**< \brief CAN Node runs at 100kBit/s. */
	CAN_SPEED_125KBPS=125, 				/**< \brief CAN Node runs at 125kBit/s. */
	CAN_SPEED_250KBPS=250, 				/**< \brief CAN Node runs at 250kBit/s. */
	CAN_SPEED_500KBPS=500, 				/**< \brief CAN Node runs at 500kBit/s. */
	CAN_SPEED_800KBPS=800, 				/**< \brief CAN Node runs at 800kBit/s. */
	CAN_SPEED_1000KBPS=1000				/**< \brief CAN Node runs at 1000kBit/s. */
}CAN_speed_t;

/** \brief CAN configuration structure */
typedef struct  {
	CAN_speed_t			speed;			/**< \brief CAN speed. */
    gpio_num_t 			tx_pin_id;		/**< \brief TX pin. */
    gpio_num_t 			rx_pin_id;		/**< \brief RX pin. */
    QueueHandle_t 		rx_queue;		/**< \brief Handler to FreeRTOS RX queue. */
}CAN_device_t;

/** \brief CAN configuration reference */
extern CAN_device_t CAN_cfg;


#endif /* __DRIVERS_CAN_CFG_H__ */
