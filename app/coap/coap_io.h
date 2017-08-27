#ifndef _COAP_IO_H
#define _COAP_IO_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "c_types.h"
//#include "lwip/ip_addr.h"
//#include "espconn.h"
#include "pdu.h"
#include "hash.h"
#include "coap_peer.h"
	
void coap_send(coap_peer_t* peer, coap_pdu_t *pdu);

int coap_send_confirmed(coap_peer_t* peer, coap_pdu_t *pdu);

#ifdef __cplusplus
}
#endif

#endif
