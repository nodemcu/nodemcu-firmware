/*
 * Filename: softuart2.h
 * Author: sjp27
 * Date: 01/03/2021
 * Description: softuart2 driver header.
 */

#ifndef __SOFTUART2_H__
#define __SOFTUART2_H__

#define SOFTUART2_MAX_BUFF 128 

typedef struct {
    uint8_t buffer[SOFTUART2_MAX_BUFF];
    int head;
    int tail;
    int len;
    int maxlen;
} circular_buffer_t;

typedef struct _softuart2_t{
    circular_buffer_t rx_buffer;
    circular_buffer_t tx_buffer;
    uint16_t baudrate;
    sint16_t need_len;
    char end_char;
    uint8_t pin_rx;
    uint8_t pin_tx;
    uint8_t pin_clk;
    uint8_t tx_timer;
    uint8_t tx_state;
    uint8_t tx_byte;
    uint8_t rx_timer;
    uint16_t rx_idle_timer;
    uint8_t rx_state;
    uint8_t rx_byte;
    void (*rx_callback)(struct _softuart2_t*);
} softuart2_t;

// driver public functions
softuart2_t *softuart2_drv_init();
bool softuart2_drv_start();
softuart2_t *softuart2_drv_get_data();
void softuart2_drv_delete();
int circular_buffer_push(circular_buffer_t *z_buffer, uint8_t z_data);
int circular_buffer_pop(circular_buffer_t *z_buffer, uint8_t *z_data);
int circular_buffer_length(circular_buffer_t *z_buffer);
#endif
