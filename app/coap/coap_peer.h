#ifndef _COAP_PEER_H
#define _COAP_PEER_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "lauxlib.h"
#include "node.h"

typedef struct {
    int sender_ref;
    int server_ref;
    coap_queue_t *queue;
    lua_State* L;
    unsigned short message_id;
} coap_peer_t;

#ifdef __cplusplus
}
#endif

#endif

