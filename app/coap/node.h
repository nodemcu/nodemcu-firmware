#ifndef _NODE_H
#define _NODE_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "hash.h"
#include "pdu.h"

struct coap_queue_t;
typedef uint32_t coap_tick_t;

/*
1. queue(first)->t store when to send PDU for the next time, it's a base(absolute) time
2. queue->next->t store the delta between time and base-time.  queue->next->t = timeout + now - basetime
3. node->next->t store the delta between time and previous->t.  node->next->t = timeout + now - node->t - basetime
4. time to fire:   10,   15,    18,    25
		node->t:   10,   5,     3,     7
*/

typedef struct coap_queue_t {
  struct coap_queue_t *next;

  coap_tick_t t;	        /**< when to send PDU for the next time */
  unsigned char retransmit_cnt;	/**< retransmission counter, will be removed when zero */
  unsigned int timeout;		/**< the randomized timeout value */

  coap_tid_t id;		/**< unique transaction id */

  // coap_packet_t *pkt;
  coap_pdu_t *pdu;		/**< the CoAP PDU to send */
  struct espconn *pconn;
} coap_queue_t;

void coap_free_node(coap_queue_t *node);

/** Adds node to given queue, ordered by node->t. */
int coap_insert_node(coap_queue_t **queue, coap_queue_t *node);

/** Destroys specified node. */
int coap_delete_node(coap_queue_t *node);

/** Removes all items from given queue and frees the allocated storage. */
void coap_delete_all(coap_queue_t *queue);

/** Creates a new node suitable for adding to the CoAP sendqueue. */
coap_queue_t *coap_new_node(void);

coap_queue_t *coap_pop_next( coap_queue_t **queue );

int coap_remove_node( coap_queue_t **queue, const coap_tid_t id);

#ifdef __cplusplus
}
#endif

#endif
