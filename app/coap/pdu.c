#include "c_stdlib.h"
#include "pdu.h"

coap_pdu_t * coap_new_pdu(void) {
  coap_pdu_t *pdu = NULL;
  pdu = (coap_pdu_t *)c_zalloc(sizeof(coap_pdu_t));
  if(!pdu){
    NODE_DBG("coap_new_pdu malloc error.\n");
    return NULL;
  }

  pdu->scratch.p = (uint8_t *)c_zalloc(MAX_REQ_SCRATCH_SIZE);
  if(!pdu->scratch.p){
    NODE_DBG("coap_new_pdu malloc error.\n");
    c_free(pdu);
    return NULL;
  }
  pdu->scratch.len = MAX_REQ_SCRATCH_SIZE;
  
  pdu->pkt = (coap_packet_t *)c_zalloc(sizeof(coap_packet_t));
  if(!pdu->pkt){
    NODE_DBG("coap_new_pdu malloc error.\n");
    c_free(pdu->scratch.p);
    c_free(pdu);
    return NULL;
  }
  pdu->pkt->content.p = NULL;
  pdu->pkt->content.len = 0;

  pdu->msg.p = (uint8_t *)c_zalloc(MAX_REQUEST_SIZE+1); // +1 for string '\0'
  if(!pdu->msg.p){
    NODE_DBG("coap_new_pdu malloc error.\n");
    c_free(pdu->pkt);
    c_free(pdu->scratch.p);
    c_free(pdu);
    return NULL;
  }
  pdu->msg.len = MAX_REQUEST_SIZE;
  return pdu;
}

void coap_delete_pdu(coap_pdu_t *pdu){
  if(!pdu)
    return;

  if(pdu->scratch.p){
    c_free(pdu->scratch.p);
    pdu->scratch.p = NULL;
    pdu->scratch.len = 0;
  }

  if(pdu->pkt){
    c_free(pdu->pkt);
    pdu->pkt = NULL;
  }

  if(pdu->msg.p){
    c_free(pdu->msg.p);
    pdu->msg.p = NULL;
    pdu->msg.len = 0;
  }

  c_free(pdu);
  pdu = NULL;
}
