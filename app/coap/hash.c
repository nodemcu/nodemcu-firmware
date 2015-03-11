#include "hash.h"
#include "c_string.h"
/* Caution: When changing this, update COAP_DEFAULT_WKC_HASHKEY
 * accordingly (see int coap_hash_path());
 */
void coap_hash(const unsigned char *s, unsigned int len, coap_key_t h) {
  size_t j;

  while (len--) {
    j = sizeof(coap_key_t)-1;
  
    while (j) {
      h[j] = ((h[j] << 7) | (h[j-1] >> 1)) + h[j];
      --j;
    }

    h[0] = (h[0] << 7) + h[0] + *s++;
  }
}

void coap_transaction_id(const uint32_t ip, const uint32_t port, const coap_packet_t *pkt, coap_tid_t *id) {
  coap_key_t h;
  c_memset(h, 0, sizeof(coap_key_t));

  /* Compare the transport address. */
  coap_hash((const unsigned char *)&(port), sizeof(port), h); 
  coap_hash((const unsigned char *)&(ip), sizeof(ip), h); 
  coap_hash((const unsigned char *)(pkt->hdr.id), sizeof(pkt->hdr.id), h);
  *id = ((h[0] << 8) | h[1]) ^ ((h[2] << 8) | h[3]);
}
