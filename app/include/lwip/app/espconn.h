/*
 * ESPRESSIF MIT License
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

#ifndef __ESPCONN_H__
#define __ESPCONN_H__

#include "lwip/err.h"
#include "lwip/dns.h"
#include "os_type.h"
#include "lwip/app/espconn_buf.h"

#if 0
#define espconn_printf(fmt, args...) os_printf(fmt,## args)
#else
#define espconn_printf(fmt, args...)
#endif


typedef void *espconn_handle;
typedef void (* espconn_connect_callback)(void *arg);
typedef void (* espconn_reconnect_callback)(void *arg, sint8 err);

/* Definitions for error constants. */

#define ESPCONN_OK          0    /* No error, everything OK. */
#define ESPCONN_MEM        -1    /* Out of memory error.     */
#define ESPCONN_TIMEOUT    -3    /* Timeout.                 */
#define ESPCONN_RTE        -4    /* Routing problem.         */
#define ESPCONN_INPROGRESS  -5   /* Operation in progress    */
#define ESPCONN_MAXNUM		-7	 /* Total number exceeds the set maximum*/

#define ESPCONN_ABRT       -8    /* Connection aborted.      */
#define ESPCONN_RST        -9    /* Connection reset.        */
#define ESPCONN_CLSD       -10   /* Connection closed.       */
#define ESPCONN_CONN       -11   /* Not connected.           */

#define ESPCONN_ARG        -12   /* Illegal argument.        */
#define ESPCONN_IF		   -14	 /* Low_level error			 */
#define ESPCONN_ISCONN     -15   /* Already connected.       */
#define ESPCONN_TIME	   -16	 /* Sync Time error			 */
#define ESPCONN_NODATA	   -17	 /* No data can be read	     */

#define ESPCONN_HANDSHAKE  -28   /* ssl handshake failed	 */
#define ESPCONN_SSL_INVALID_DATA  -61   /* ssl application invalid	 */

#define ESPCONN_SSL			0x01
#define ESPCONN_NORM		0x00

#define ESPCONN_STA			0x01
#define ESPCONN_AP			0x02
#define ESPCONN_AP_STA		0x03

#define STA_NETIF      0x00
#define AP_NETIF       0x01

/** Protocol family and type of the espconn */
enum espconn_type {
    ESPCONN_INVALID    = 0,
    /* ESPCONN_TCP Group */
    ESPCONN_TCP        = 0x10,
    /* ESPCONN_UDP Group */
    ESPCONN_UDP        = 0x20,
};

/** Current state of the espconn. Non-TCP espconn are always in state ESPCONN_NONE! */
enum espconn_state {
    ESPCONN_NONE,
    ESPCONN_WAIT,
    ESPCONN_LISTEN,
    ESPCONN_CONNECT,
    ESPCONN_WRITE,
    ESPCONN_READ,
    ESPCONN_CLOSE
};

typedef struct _esp_tcp {
    int remote_port;
    int local_port;
    uint8 local_ip[4];
    uint8 remote_ip[4];
	espconn_connect_callback connect_callback;
	espconn_reconnect_callback reconnect_callback;
	espconn_connect_callback disconnect_callback;
	espconn_connect_callback write_finish_fn;
} esp_tcp;

typedef struct _esp_udp {
    int remote_port;
    int local_port;
    uint8 local_ip[4];
	uint8 remote_ip[4];
} esp_udp;

typedef struct _remot_info{
	enum espconn_state state;
	int remote_port;
	uint8 remote_ip[4];
}remot_info;

/** A callback prototype to inform about events for a espconn */
typedef void (* espconn_recv_callback)(void *arg, char *pdata, unsigned short len);
typedef void (* espconn_sent_callback)(void *arg);

/** A espconn descriptor */
struct espconn {
    /** type of the espconn (TCP, UDP) */
    enum espconn_type type;
    /** current state of the espconn */
    enum espconn_state state;
    union {
        esp_tcp *tcp;
        esp_udp *udp;
    } proto;
    /** A callback function that is informed about events for this espconn */
    espconn_recv_callback recv_callback;
	espconn_sent_callback sent_callback;
	uint8 link_cnt;
	void *reverse;
};

enum espconn_option{
	ESPCONN_START = 0x00,
	ESPCONN_REUSEADDR = 0x01,
	ESPCONN_NODELAY = 0x02,
	ESPCONN_COPY = 0x04,
	ESPCONN_KEEPALIVE = 0x08,
	ESPCONN_MANUALRECV = 0x10,
	ESPCONN_END
};

enum espconn_mode{
	ESPCONN_NOMODE,
	ESPCONN_TCPSERVER_MODE,
	ESPCONN_TCPCLIENT_MODE,
	ESPCONN_UDP_MODE,
	ESPCONN_NUM_MODE
};

struct espconn_packet{
	uint16 sent_length;		/* sent length successful*/
	uint16 snd_buf_size;	/* Available buffer size for sending  */
	uint16 snd_queuelen;	/* Available buffer space for sending */
	uint16 total_queuelen;	/* total Available buffer space for sending */
	uint32 packseqno;		/* seqno to be sent */
	uint32 packseq_nxt;		/* seqno expected */
	uint32 packnum;
};

typedef struct _espconn_buf{
	uint8 *payload;
	uint8 *punsent;
	uint16 unsent;
	uint16 len;
	uint16 tot_len;
	struct _espconn_buf *pnext;
} espconn_buf;

typedef struct _comon_pkt{
	void *pcb;
	int remote_port;
	uint8 remote_ip[4];
	uint32 local_port;
	uint32 local_ip;
	espconn_buf *pbuf;
	espconn_buf *ptail;
	uint8* ptrbuf;
	uint16 cntr;
	sint8  err;
	uint32 timeout;
	uint32 recv_check;
	uint8  pbuf_num;
	struct espconn_packet packet_info;
	bool write_flag;
	enum espconn_option espconn_opt;
}comon_pkt;

typedef struct _espconn_msg{
	struct espconn *pespconn;
	comon_pkt pcommon;
	uint8 count_opt;
	uint8 espconn_mode;
	sint16_t hs_status;	//the status of the handshake
	void *preverse;
	void *pssl;
	struct _espconn_msg *pnext;

//***********Code for WIFI_BLOCK from upper**************
	uint8 recv_hold_flag;
	uint16 recv_holded_buf_Len;
//*******************************************************
	ringbuf *readbuf;
}espconn_msg;

#ifndef _MDNS_INFO
#define _MDNS_INFO
struct mdns_info {
	char *host_name;
	char *server_name;
	uint16 server_port;
	unsigned long ipAddr;
	char *txt_data[10];
};
#endif

#define linkMax 15

#define   espconn_delay_disabled(espconn)  (((espconn)->pcommon.espconn_opt & ESPCONN_NODELAY) != 0)
#define   espconn_delay_enabled(espconn)  (((espconn)->pcommon.espconn_opt & ESPCONN_NODELAY) == 0)
#define   espconn_reuse_disabled(espconn)  (((espconn)->pcommon.espconn_opt & ESPCONN_REUSEADDR) != 0)
#define   espconn_copy_disabled(espconn)  (((espconn)->pcommon.espconn_opt & ESPCONN_COPY) != 0)
#define   espconn_copy_enabled(espconn)  (((espconn)->pcommon.espconn_opt & ESPCONN_COPY) == 0)
#define   espconn_keepalive_disabled(espconn)  (((espconn)->pcommon.espconn_opt & ESPCONN_KEEPALIVE) != 0)
#define   espconn_keepalive_enabled(espconn)  (((espconn)->pcommon.espconn_opt & ESPCONN_KEEPALIVE) == 0)

#define espconn_TaskPrio        26
#define espconn_TaskQueueLen    15

enum espconn_sig {
    SIG_ESPCONN_NONE,
    SIG_ESPCONN_ERRER,
    SIG_ESPCONN_LISTEN,
    SIG_ESPCONN_CONNECT,
    SIG_ESPCONN_WRITE,
    SIG_ESPCONN_SEND,
    SIG_ESPCONN_READ,
    SIG_ESPCONN_CLOSE
};

/******************************************************************************
 * FunctionName : espconn_copy_partial
 * Description  : reconnect with host
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/

void espconn_copy_partial(struct espconn *pesp_dest, struct espconn *pesp_source);

/******************************************************************************
 * FunctionName : espconn_copy_partial
 * Description  : insert the node to the active connection list
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/

void espconn_list_creat(espconn_msg **phead, espconn_msg* pinsert);

/******************************************************************************
 * FunctionName : espconn_list_delete
 * Description  : remove the node from the active connection list
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/

void espconn_list_delete(espconn_msg **phead, espconn_msg* pdelete);

/******************************************************************************
 * FunctionName : espconn_find_connection
 * Description  : Initialize the server: set up a listening PCB and bind it to
 *                the defined port
 * Parameters   : espconn -- the espconn used to build server
 * Returns      : none
 *******************************************************************************/

bool espconn_find_connection(struct espconn *pespconn, espconn_msg **pnode);

/******************************************************************************
 * FunctionName : espconn_get_connection_info
 * Description  : used to specify the function that should be called when disconnect
 * Parameters   : espconn -- espconn to set the err callback
 *                discon_cb -- err callback function to call when err
 * Returns      : none
*******************************************************************************/

sint8 espconn_get_connection_info(struct espconn *pespconn, remot_info **pcon_info, uint8 typeflags);

/******************************************************************************
 * FunctionName : espconn_connect
 * Description  : The function given as the connect
 * Parameters   : espconn -- the espconn used to listen the connection
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_connect(struct espconn *espconn);

/******************************************************************************
 * FunctionName : espconn_disconnect
 * Description  : disconnect with host
 * Parameters   : espconn -- the espconn used to disconnect the connection
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_disconnect(struct espconn *espconn);

/******************************************************************************
 * FunctionName : espconn_delete
 * Description  : disconnect with host
 * Parameters   : espconn -- the espconn used to disconnect the connection
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_delete(struct espconn *espconn);

/******************************************************************************
 * FunctionName : espconn_accept
 * Description  : The function given as the listen
 * Parameters   : espconn -- the espconn used to listen the connection
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_accept(struct espconn *espconn);

/******************************************************************************
 * FunctionName : espconn_create
 * Description  : sent data for client or server
 * Parameters   : espconn -- espconn to the data transmission
 * Returns      : result
*******************************************************************************/

extern sint8 espconn_create(struct espconn *espconn);

/******************************************************************************
 * FunctionName : espconn_tcp_get_max_con
 * Description  : get the number of simulatenously active TCP connections
 * Parameters   : none
 * Returns      : none
*******************************************************************************/

extern uint8 espconn_tcp_get_max_con(void);

/******************************************************************************
 * FunctionName : espconn_tcp_get_max_con_allow
 * Description  : get the count of simulatenously active connections on the server
 * Parameters   : espconn -- espconn to get the count
 * Returns      : result
*******************************************************************************/

extern sint8 espconn_tcp_get_max_con_allow(struct espconn *espconn);

/******************************************************************************
 * FunctionName : espconn_tcp_set_buf_count
 * Description  : set the total number of espconn_buf on the unsent lists
 * Parameters   : espconn -- espconn to set the count
 * 				  num -- the total number of espconn_buf
 * Returns      : result
*******************************************************************************/

extern sint8 espconn_tcp_set_buf_count(struct espconn *espconn, uint8 num);

/******************************************************************************
 * FunctionName : espconn_regist_time
 * Description  : used to specify the time that should be called when don't recv data
 * Parameters   : espconn -- the espconn used to the connection
 * 				  interval -- the timer when don't recv data
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_regist_time(struct espconn *espconn, uint32 interval, uint8 type_flag);

/******************************************************************************
 * FunctionName : espconn_regist_sentcb
 * Description  : Used to specify the function that should be called when data
 * 				  has been successfully delivered to the remote host.
 * Parameters   : struct espconn *espconn -- espconn to set the sent callback
 * 				  espconn_sent_callback sent_cb -- sent callback function to
 * 				  call for this espconn when data is successfully sent
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_regist_sentcb(struct espconn *espconn, espconn_sent_callback sent_cb);

/******************************************************************************
 * FunctionName : espconn_regist_sentcb
 * Description  : Used to specify the function that should be called when data
 *                has been successfully delivered to the remote host.
 * Parameters   : espconn -- espconn to set the sent callback
 *                sent_cb -- sent callback function to call for this espconn
 *                when data is successfully sent
 * Returns      : none
*******************************************************************************/
extern sint8 espconn_regist_write_finish(struct espconn *espconn, espconn_connect_callback write_finish_fn);

/******************************************************************************
 * FunctionName : espconn_send
 * Description  : sent data for client or server
 * Parameters   : espconn -- espconn to set for client or server
 *                psent -- data to send
 *                length -- length of data to send
 * Returns      : none
*******************************************************************************/
extern sint8 espconn_send(struct espconn *espconn, uint8 *psent, uint16 length);


/******************************************************************************
 * FunctionName : espconn_sent
 * Description  : sent data for client or server
 * Parameters   : espconn -- espconn to set for client or server
 * 				  psent -- data to send
 *                length -- length of data to send
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_sent(struct espconn *espconn, uint8 *psent, uint16 length);

/******************************************************************************
 * FunctionName : espconn_regist_connectcb
 * Description  : used to specify the function that should be called when
 * 				  connects to host.
 * Parameters   : espconn -- espconn to set the connect callback
 * 				  connect_cb -- connected callback function to call when connected
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_regist_connectcb(struct espconn *espconn, espconn_connect_callback connect_cb);

/******************************************************************************
 * FunctionName : espconn_regist_recvcb
 * Description  : used to specify the function that should be called when recv
 * 				  data from host.
 * Parameters   : espconn -- espconn to set the recv callback
 * 				  recv_cb -- recv callback function to call when recv data
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_regist_recvcb(struct espconn *espconn, espconn_recv_callback recv_cb);

/******************************************************************************
 * FunctionName : espconn_regist_reconcb
 * Description  : used to specify the function that should be called when connection
 * 				  because of err disconnect.
 * Parameters   : espconn -- espconn to set the err callback
 * 				  recon_cb -- err callback function to call when err
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_regist_reconcb(struct espconn *espconn, espconn_reconnect_callback recon_cb);

/******************************************************************************
 * FunctionName : espconn_regist_disconcb
 * Description  : used to specify the function that should be called when disconnect
 * Parameters   : espconn -- espconn to set the err callback
 *                discon_cb -- err callback function to call when err
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_regist_disconcb(struct espconn *espconn, espconn_connect_callback discon_cb);

/******************************************************************************
 * FunctionName : espconn_port
 * Description  : access port value for client so that we don't end up bouncing
 *                all connections at the same time .
 * Parameters   : none
 * Returns      : access port value
*******************************************************************************/

extern uint32 espconn_port(void);

/******************************************************************************
 * FunctionName : espconn_encry_connect
 * Description  : The function given as connection
 * Parameters   : espconn -- the espconn used to connect with the host
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_secure_connect(struct espconn *espconn);

/******************************************************************************
 * FunctionName : espconn_encry_disconnect
 * Description  : The function given as the disconnection
 * Parameters   : espconn -- the espconn used to disconnect with the host
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_secure_disconnect(struct espconn *espconn);

/******************************************************************************
 * FunctionName : espconn_secure_send
 * Description  : sent data for client or server
 * Parameters   : espconn -- espconn to set for client or server
 *                               psent -- data to send
 *                length -- length of data to send
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_secure_send(struct espconn *espconn, uint8 *psent, uint16 length);

/******************************************************************************
 * FunctionName : espconn_encry_sent
 * Description  : sent data for client or server
 * Parameters   : espconn -- espconn to set for client or server
 *                               psent -- data to send
 *                length -- length of data to send
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_secure_sent(struct espconn *espconn, uint8 *psent, uint16 length);

/******************************************************************************
 * FunctionName : espconn_secure_ca_enable
 * Description  : enable the certificate authenticate and set the flash sector
 *                               as client or server
 * Parameters   : level -- set for client or server
 *                               1: client,2:server,3:client and server
 *                               flash_sector -- flash sector for save certificate
 * Returns      : result true or false
*******************************************************************************/

extern bool espconn_secure_ca_enable(uint8 level, uint32 flash_sector );

/******************************************************************************
 * FunctionName : espconn_secure_ca_disable
 * Description  : disable the certificate authenticate  as client or server
 * Parameters   : level -- set for client or server
 *                               1: client,2:server,3:client and server
 * Returns      : result true or false
*******************************************************************************/

extern bool espconn_secure_ca_disable(uint8 level);


/******************************************************************************
 * FunctionName : espconn_secure_cert_req_enable
 * Description  : enable the client certificate authenticate and set the flash sector
 *                               as client or server
 * Parameters   : level -- set for client or server
 *                               1: client,2:server,3:client and server
 *                               flash_sector -- flash sector for save certificate
 * Returns      : result true or false
*******************************************************************************/

extern bool espconn_secure_cert_req_enable(uint8 level, uint32 flash_sector );

/******************************************************************************
 * FunctionName : espconn_secure_ca_disable
 * Description  : disable the client certificate authenticate  as client or server
 * Parameters   : level -- set for client or server
 *                               1: client,2:server,3:client and server
 * Returns      : result true or false
*******************************************************************************/

extern bool espconn_secure_cert_req_disable(uint8 level);

/******************************************************************************
 * FunctionName : espconn_recv_hold
 * Description  : hold tcp receive
 * Parameters   : espconn -- espconn to hold
 * Returns      : none
*******************************************************************************/
extern sint8 espconn_recv_hold(struct espconn *pespconn);

/******************************************************************************
 * FunctionName : espconn_recv_unhold
 * Description  : unhold tcp receive
 * Parameters   : espconn -- espconn to unhold
 * Returns      : none
*******************************************************************************/
extern sint8 espconn_recv_unhold(struct espconn *pespconn);

#endif

