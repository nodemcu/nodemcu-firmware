#include "c_string.h"
#include "coap_io.h"
#include "node.h"
#include "espconn.h"
#include "coap_timer.h"

extern coap_queue_t *gQueue;

/* releases space allocated by PDU if free_pdu is set */
coap_tid_t coap_send(struct espconn *pesp_conn, coap_pdu_t *pdu) {
  coap_tid_t id = COAP_INVALID_TID;
  uint32_t ip = 0, port = 0;
  if ( !pesp_conn || !pdu )
    return id;

  espconn_sent(pesp_conn, (unsigned char *)(pdu->msg.p), pdu->msg.len);

  if(pesp_conn->type == ESPCONN_TCP){
    c_memcpy(&ip, pesp_conn->proto.tcp->remote_ip, sizeof(ip));
    port = pesp_conn->proto.tcp->remote_port;
  }else{
    c_memcpy(&ip, pesp_conn->proto.udp->remote_ip, sizeof(ip));
    port = pesp_conn->proto.udp->remote_port;
  }
  coap_transaction_id(ip, port, pdu->pkt, &id);
  return id;
}

coap_tid_t coap_send_confirmed(struct espconn *pesp_conn, coap_pdu_t *pdu) {
  coap_queue_t *node;
  coap_tick_t diff;
  uint32_t r;

  node = coap_new_node();
  if (!node) {
    NODE_DBG("coap_send_confirmed: insufficient memory\n");
    return COAP_INVALID_TID;
  }

  node->retransmit_cnt = 0;
  node->id = coap_send(pesp_conn, pdu);
  if (COAP_INVALID_TID == node->id) {
    NODE_DBG("coap_send_confirmed: error sending pdu\n");
    coap_free_node(node);
    return COAP_INVALID_TID;
  }
  r = rand();

  /* add randomized RESPONSE_TIMEOUT to determine retransmission timeout */
  node->timeout = COAP_DEFAULT_RESPONSE_TIMEOUT * COAP_TICKS_PER_SECOND +
    (COAP_DEFAULT_RESPONSE_TIMEOUT >> 1) *
    ((COAP_TICKS_PER_SECOND * (r & 0xFF)) >> 8);

  node->pconn = pesp_conn;
  node->pdu = pdu;

  /* Set timer for pdu retransmission. If this is the first element in
   * the retransmission queue, the base time is set to the current
   * time and the retransmission time is node->timeout. If there is
   * already an entry in the sendqueue, we must check if this node is
   * to be retransmitted earlier. Therefore, node->timeout is first
   * normalized to the timeout and then inserted into the queue with
   * an adjusted relative time.
   */
  coap_timer_stop();
  coap_timer_update(&gQueue);
  node->t = node->timeout;  
  coap_insert_node(&gQueue, node);
  coap_timer_start(&gQueue);
  return node->id;
}
