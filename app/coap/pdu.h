#ifndef _PDU_H
#define _PDU_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "coap.h"

/** Header structure for CoAP PDUs */
typedef struct {
  coap_rw_buffer_t scratch;
  //packet and message
  coap_packet_t *pkt;
  coap_rw_buffer_t msg;   /**< the CoAP msg to send */
  //ip and port
  char ip[17];
  int port;
  int response_ref;
} coap_pdu_t;

coap_pdu_t *coap_new_pdu(void);

void coap_delete_pdu(coap_pdu_t *pdu);

#ifdef __cplusplus
}
#endif

#endif
