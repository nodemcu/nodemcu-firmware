#include "user_config.h"
#include "c_types.h"

#include "coap.h"

size_t coap_server_respond(char *data, unsigned short len, unsigned short size)
{
  NODE_DBG("coap_server_respond is called.\n");
  if(len>size){
    NODE_DBG("len:%d, size:%d\n",len,size);
    return 0;
  }
  size_t rsplen = size;
  coap_packet_t pkt;
  uint8_t scratch_raw[4];
  coap_rw_buffer_t scratch_buf = {scratch_raw, sizeof(scratch_raw)};
  int rc;

#ifdef COAP_DEBUG
  NODE_DBG("Received: ");
  coap_dump(data, len, true);
  NODE_DBG("\n");
#endif

  if (0 != (rc = coap_parse(&pkt, data, len))){
    NODE_DBG("Bad packet rc=%d\n", rc);
    return 0;
  }
  else
  {
    coap_packet_t rsppkt;
#ifdef COAP_DEBUG
    coap_dumpPacket(&pkt);
#endif
    coap_handle_req(&scratch_buf, &pkt, &rsppkt);
    if (0 != (rc = coap_build(data, &rsplen, &rsppkt))){
      NODE_DBG("coap_build failed rc=%d\n", rc);
      return 0;
    }
    else
    {
#ifdef COAP_DEBUG
      NODE_DBG("Responding: ");
      coap_dump(data, rsplen, true);
      NODE_DBG("\n");
#endif
#ifdef COAP_DEBUG
      coap_dumpPacket(&rsppkt);
#endif
    }
    return rsplen;
  }
}
