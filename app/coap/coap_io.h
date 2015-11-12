#ifndef _COAP_IO_H
#define _COAP_IO_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "c_types.h"
#include "lwip/ip_addr.h"
#include "espconn.h"
#include "pdu.h"
#include "hash.h"
	
coap_tid_t coap_send(struct espconn *pesp_conn, coap_pdu_t *pdu);

coap_tid_t coap_send_confirmed(struct espconn *pesp_conn, coap_pdu_t *pdu);

#ifdef __cplusplus
}
#endif

#endif
