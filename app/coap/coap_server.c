#include "user_config.h"
#include "c_types.h"
#include "c_stdlib.h"

#include "coap.h"

size_t coap_server_respond(char *req, unsigned short reqlen, char *rsp, unsigned short rsplen)
{
  NODE_DBG("coap_server_respond is called.\n");
  size_t rlen = rsplen;
  coap_packet_t pkt;
  pkt.content.p = NULL;
  pkt.content.len = 0;
  uint8_t scratch_raw[4];
  coap_rw_buffer_t scratch_buf = {scratch_raw, sizeof(scratch_raw)};
  int rc;

#ifdef COAP_DEBUG
  NODE_DBG("Received: ");
  coap_dump(req, reqlen, true);
  NODE_DBG("\n");
#endif

  if (0 != (rc = coap_parse(&pkt, req, reqlen))){
    NODE_DBG("Bad packet rc=%d\n", rc);
    return 0;
  }
  else
  {
    coap_packet_t rsppkt;
    rsppkt.content.p = NULL;
    rsppkt.content.len = 0;
#ifdef COAP_DEBUG
    coap_dumpPacket(&pkt);
#endif
    coap_handle_req(&scratch_buf, &pkt, &rsppkt);
    if (0 != (rc = coap_build(rsp, &rlen, &rsppkt))){
      NODE_DBG("coap_build failed rc=%d\n", rc);
      // return 0;
      rlen = 0;
    }
    else
    {
#ifdef COAP_DEBUG
      NODE_DBG("Responding: ");
      coap_dump(rsp, rlen, true);
      NODE_DBG("\n");
#endif
#ifdef COAP_DEBUG
      coap_dumpPacket(&rsppkt);
#endif
    }
    if(rsppkt.content.p){
      c_free(rsppkt.content.p);
      rsppkt.content.p = NULL;
      rsppkt.content.len = 0;
    }
    return rlen;
  }
}
