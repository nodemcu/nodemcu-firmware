#ifndef _COAP_TIMER_H
#define _COAP_TIMER_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "node.h"

#define SYS_TIME_MAX (0xFFFFFFFF / 1000)

#define COAP_DEFAULT_RESPONSE_TIMEOUT  2 /* response timeout in seconds */
#define COAP_DEFAULT_MAX_RETRANSMIT    4 /* max number of retransmissions */
#define COAP_TICKS_PER_SECOND 1000    // ms
#define DEFAULT_MAX_TRANSMIT_WAIT   90

void coap_timer_elapsed(coap_tick_t *diff);

void coap_timer_setup(coap_queue_t ** queue, coap_tick_t t);

void coap_timer_stop(void);

void coap_timer_update(coap_queue_t ** queue);

void coap_timer_start(coap_queue_t ** queue);

#ifdef __cplusplus
}
#endif

#endif
