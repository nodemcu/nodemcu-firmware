#ifndef _HASH_H
#define _HASH_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "coap.h"

typedef unsigned char coap_key_t[4];

/* CoAP transaction id */
/*typedef unsigned short coap_tid_t; */
typedef int coap_tid_t;
#define COAP_INVALID_TID -1

void coap_transaction_id(const uint32_t ip, const uint32_t port, const coap_packet_t *pkt, coap_tid_t *id);

#ifdef __cplusplus
}
#endif

#endif
