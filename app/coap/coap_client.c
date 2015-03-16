#include "user_config.h"
#include "c_types.h"

#include "coap.h"
#include "hash.h"
#include "node.h"

extern coap_queue_t *gQueue;

void coap_client_response_handler(char *data, unsigned short len, unsigned short size, const uint32_t ip, const uint32_t port)
{
  NODE_DBG("coap_client_response_handler is called.\n");
  coap_packet_t pkt;
  pkt.content.p = NULL;
  pkt.content.len = 0;
  int rc;

  if (0 != (rc = coap_parse(&pkt, data, len))){
    NODE_DBG("Bad packet rc=%d\n", rc);
  }
  else
  {
#ifdef COAP_DEBUG
    coap_dumpPacket(&pkt);
#endif
    /* check if this is a response to our original request */
    if (!check_token(&pkt)) {
      /* drop if this was just some message, or send RST in case of notification */
      if (pkt.hdr.t == COAP_TYPE_CON || pkt.hdr.t == COAP_TYPE_NONCON){
        // coap_send_rst(pkt);  // send RST response
        // or, just ignore it.
      }
      goto end;
    }

    if (pkt.hdr.t == COAP_TYPE_RESET) {
      NODE_DBG("got RST\n");
      goto end;
    }

    coap_tid_t id = COAP_INVALID_TID;
    coap_transaction_id(ip, port, &pkt, &id);
    /* transaction done, remove the node from queue */
    // stop timer
    coap_timer_stop();
    // remove the node
    coap_remove_node(&gQueue, id);
    // calculate time elapsed
    coap_timer_update(&gQueue);
    coap_timer_start(&gQueue);

    if (COAP_RESPONSE_CLASS(pkt.hdr.code) == 2)
    {
      /* There is no block option set, just read the data and we are done. */
      NODE_DBG("%d.%02d\t", (pkt.hdr.code >> 5), pkt.hdr.code & 0x1F);
      NODE_DBG((char *)(pkt.payload.p));
    }
    else if (COAP_RESPONSE_CLASS(pkt.hdr.code) >= 4)
    {
      NODE_DBG("%d.%02d\t", (pkt.hdr.code >> 5), pkt.hdr.code & 0x1F);
      NODE_DBG((char *)(pkt.payload.p));
    }
  }

end:
  if(!gQueue){ // if there is no node pending in the queue, disconnect from host.

  }
}
