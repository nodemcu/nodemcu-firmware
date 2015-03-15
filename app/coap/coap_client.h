#ifndef _COAP_CLIENT_H
#define _COAP_CLIENT_H 1

#ifdef __cplusplus
extern "C" {
#endif

void coap_client_response_handler(char *data, unsigned short len, unsigned short size, const uint32_t ip, const uint32_t port);

#ifdef __cplusplus
}
#endif

#endif
