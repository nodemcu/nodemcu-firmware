#include "c_string.h"
#include "coap_io.h"
#include "node.h"
#include "coap_timer.h"
#include "lwip/ip_addr.h"

//extern coap_queue_t *gQueue;

void coap_send(coap_peer_t* peer, coap_pdu_t *pdu){
    lua_State* L = peer->L;
#ifdef COAP_DEBUG
    coap_dumpPacket(pdu->pkt);
#endif
    NODE_DBG("send message");
    lua_rawgeti (L,LUA_REGISTRYINDEX,peer->sender_ref);
    lua_pushlstring(L,pdu->msg.p,pdu->msg.len);
    lua_pushstring(L,pdu->ip);
    lua_pushinteger(L,pdu->port);
    lua_call(L,3,0);
}

int coap_send_confirmed(coap_peer_t* peer, coap_pdu_t *pdu){
    coap_queue_t *node;
    coap_tick_t diff;
    uint32_t r;
    ip_addr_t ipaddr;
    int bip;
    
    NODE_DBG("send comfired message");
    
    node = coap_new_node();
    if(!node){
        NODE_DBG("coap_send_confirmed: insufficient memory\n");
        return 0;
    }
    node->retransmit_cnt = 0;
    coap_send(peer,pdu);
    ipaddr.addr = ipaddr_addr(pdu->ip);
    c_memcpy(&bip, &ipaddr.addr, 4);
    
    coap_transaction_id(bip, pdu->port, pdu->pkt, &node->id);
    NODE_DBG("transaction id is %d\n",node->id);
    r = rand();
    node->timeout = COAP_DEFAULT_RESPONSE_TIMEOUT * COAP_TICKS_PER_SECOND +
        (COAP_DEFAULT_RESPONSE_TIMEOUT >> 1) *
        ((COAP_TICKS_PER_SECOND * (r & 0xFF)) >> 8);
    node->pdu = pdu;
    coap_timer_stop();
    coap_timer_update(peer);
    node->t = node->timeout;  
    coap_insert_node(&peer->queue, node);
    coap_timer_start(peer);
    return 1;
}
