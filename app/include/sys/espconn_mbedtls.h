/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2016 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef ESPCONN_MBEDTLS_H_
#define ESPCONN_MBEDTLS_H_

#include "lwip/ip.h"
#include "lwip/app/espconn.h"
#include "user_interface.h"

#if !defined(ESPCONN_MBEDTLS)

#include "mbedtls/net_sockets.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
typedef struct espconn *pmbedtls_espconn;
typedef struct espconn mbedtls_espconn;
typedef struct{
	int record_len;
}mbedtls_record;

#if defined(ESP8266_PLATFORM)
typedef struct{
	uint8*	finished_buf;
	int 	finished_len;
}mbedtls_finished, *pmbedtls_finished;
#endif

typedef struct{
//	mbedtls_entropy_context entropy;
	mbedtls_x509_crt cacert;
	mbedtls_x509_crt clicert;
	mbedtls_pk_context pkey;
}mbedtls_session, *pmbedtls_session;

typedef struct{
	bool quiet;
	mbedtls_record		record;
#if defined(ESP8266_PLATFORM)
	pmbedtls_finished	pfinished;
#endif
	pmbedtls_session	psession;
	mbedtls_net_context fd;
	mbedtls_net_context listen_fd;	
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	mbedtls_entropy_context entropy;

	bool SentFnFlag;
	sint32 verify_result;
}mbedtls_msg, *pmbedtls_msg;

typedef enum {
	ESPCONN_CERT_OWN,
	ESPCONN_CERT_AUTH,
	ESPCONN_PK,
	ESPCONN_PASSWORD
}mbedtls_auth_type;

typedef enum {
	ESPCONN_IDLE = 0,
	ESPCONN_CLIENT,
	ESPCONN_SERVER,
	ESPCONN_BOTH,
	ESPCONN_MAX
}espconn_level;

typedef struct _file_head{
	char file_name[32];
	uint16_t file_length;
}file_head;

typedef struct _file_param{
	file_head file_head;
	int32 file_offerset;
}file_param;

typedef struct _ssl_sector{
	uint32 sector;
	bool flag;
}ssl_sector;

struct ssl_packet{
	uint8* pbuffer;
	uint16 buffer_size;
	ssl_sector cert_ca_sector;
	ssl_sector cert_req_sector;
};

typedef struct _ssl_opt {
	struct ssl_packet server;
	struct ssl_packet client;
	uint8 type;
}ssl_opt;

typedef struct{
	mbedtls_auth_type auth_type;
	espconn_level	auth_level;
}mbedtls_auth_info;

#define SSL_KEEP_INTVL  1
#define SSL_KEEP_CNT	5
#define SSL_KEEP_IDLE	90

#define  ssl_keepalive_enable(pcb)   ((pcb)->so_options |= SOF_KEEPALIVE)
#define  ssl_keepalive_disable(pcb)   ((pcb)->so_options &= ~SOF_KEEPALIVE)

enum {
	SIG_ESPCONN_TLS_ERRER = 0x3B
};

#define ESPCONN_SECURE_MAX_SIZE 8192
#define ESPCONN_SECURE_DEFAULT_HEAP 0x3800
#define ESPCONN_SECURE_DEFAULT_SIZE 0x0800
#define ESPCONN_HANDSHAKE_TIMEOUT 0x3C
#define ESPCONN_INVALID_TYPE	0xFFFFFFFF
#define MBEDTLS_SSL_PLAIN_ADD	TCP_MSS
#define FLASH_SECTOR_SIZE		4096

extern ssl_opt ssl_option;

typedef struct{
	uint32 parame_sec;
	uint32 parame_type;
	uint32 parame_datalen;
	char*  parame_data;
}mbedtls_parame, *pmbedtls_parame;

/*
* Storage format identifiers
* Recognized formats: PEM and DER
*/
typedef enum{
	 ESPCONN_FORMAT_INIT = 0,
	 ESPCONN_FORMAT_DER = 1,
	 ESPCONN_FORMAT_PEM = 2,
	 ESPCONN_FORMAT_INVALID
}espconn_format;

#define ESPCONN_EVENT_RECV(pcb,p,err)                          \
  do {                                                         \
    if((pcb)!= NULL && (pcb)->recv_callback != NULL) {                                  \
		(pcb)->state = ESPCONN_READ;	\
		(pcb)->recv_callback((pcb),(p),(err));\
		(pcb)->state = ESPCONN_CONNECT;	\
    } else {                                                   \
      ESP_LOG("%s %d\n", __FILE__, __LINE__);          \
    }                                                          \
  } while (0)

#define ESPCONN_EVENT_SEND(pcb)                          \
  do {                                                         \
    if((pcb)!= NULL && (pcb)->sent_callback != NULL) {                                  \
		(pcb)->state = ESPCONN_CONNECT;	\
		(pcb)->sent_callback(pcb);\
    } else {                                                   \
      ESP_LOG("%s %d\n", __FILE__, __LINE__);          \
    }                                                          \
  } while (0)

#define ESPCONN_EVENT_CONNECTED(pcb)                          \
  do {                                                         \
    if((pcb)!= NULL && (pcb)->proto.tcp != NULL && (pcb)->proto.tcp->connect_callback != NULL) {                                  \
		(pcb)->state = ESPCONN_CONNECT;	\
		(pcb)->proto.tcp->connect_callback(pcb);\
    } else {                                                   \
      ESP_LOG("%s %d\n", __FILE__, __LINE__);          \
    }                                                          \
  } while (0)

#define ESPCONN_EVENT_CLOSED(pcb)                          \
  do {                                                         \
    if((pcb)!= NULL && (pcb)->proto.tcp != NULL && (pcb)->proto.tcp->disconnect_callback != NULL) {                                  \
		(pcb)->state = ESPCONN_CLOSE;	\
		(pcb)->proto.tcp->disconnect_callback(pcb);\
    } else {                                                   \
      ESP_LOG("%s %d\n", __FILE__, __LINE__);          \
    }                                                          \
  } while (0)

#define ESPCONN_EVENT_ERROR(pcb,err)                          \
  do {                                                         \
    if((pcb)!= NULL && (pcb)->proto.tcp != NULL && (pcb)->proto.tcp->reconnect_callback != NULL) {                                  \
		(pcb)->state = ESPCONN_CLOSE;	\
		(pcb)->proto.tcp->reconnect_callback(pcb,err);\
    } else {                                                   \
      ESP_LOG("%s %d\n", __FILE__, __LINE__);          \
    }                                                          \
  } while (0)

/******************************************************************************
 * FunctionName : mbedtls_load_default_obj
 * Description  : Initialize the server: set up a listen PCB and bind it to
 *                the defined port
 * Parameters   : espconn -- the espconn used to build client
 * Returns      : none
*******************************************************************************/
bool mbedtls_load_default_obj(uint32 flash_sector, int obj_type, const unsigned char *load_buf, uint16 length);

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

/******************************************************************************
 * FunctionName : espconn_secure_get_size
 * Description  : get buffer size for client or server
 * Parameters   : level -- set for client or server
 *				  1: client,2:server,3:client and server
 * Returns      : buffer size for client or server
*******************************************************************************/

extern sint16 espconn_secure_get_size(uint8 level);

#endif



#endif /* ESPCONN_MBEDTLS_H_ */
