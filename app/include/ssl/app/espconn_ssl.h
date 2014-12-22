#ifndef ESPCONN_SSL_CLIENT_H
#define ESPCONN_SSL_CLIENT_H

#include "ssl/ssl_ssl.h"
#include "ssl/ssl_tls1.h"

#include "lwip/app/espconn.h"

typedef struct _ssl_msg {
    SSL_CTX *ssl_ctx;
    SSL *ssl;
    bool quiet;
    char *private_key_file;
    uint8_t session_id[SSL_SESSION_ID_SIZE];
    u16_t pkt_length;
} ssl_msg;

/******************************************************************************
 * FunctionName : sslserver_start
 * Description  : Initialize the server: set up a listen PCB and bind it to 
 *                the defined port
 * Parameters   : espconn -- the espconn used to build client
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_ssl_server(struct espconn *espconn);

/******************************************************************************
 * FunctionName : espconn_ssl_client
 * Description  : Initialize the client: set up a connect PCB and bind it to 
 *                the defined port
 * Parameters   : espconn -- the espconn used to build client
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_ssl_client(struct espconn *espconn);

/******************************************************************************
 * FunctionName : espconn_ssl_write
 * Description  : sent data for client or server
 * Parameters   : void *arg -- client or server to send
 * 				  uint8* psent -- Data to send
 *                uint16 length -- Length of data to send
 * Returns      : none
*******************************************************************************/

extern void espconn_ssl_sent(void *arg, uint8 *psent, uint16 length);

/******************************************************************************
 * FunctionName : espconn_ssl_disconnect
 * Description  : A new incoming connection has been disconnected.
 * Parameters   : espconn -- the espconn used to disconnect with host
 * Returns      : none
*******************************************************************************/

extern void espconn_ssl_disconnect(espconn_msg *pdis);

#endif

