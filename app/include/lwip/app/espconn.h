#ifndef __ESPCONN_H__
#define __ESPCONN_H__

#include "lwip/dns.h"
#include "os_type.h"

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
#define ESPCONN_INPROGRESS  -5    /* Operation in progress    */

#define ESPCONN_ABRT       -8    /* Connection aborted.      */
#define ESPCONN_RST        -9    /* Connection reset.        */
#define ESPCONN_CLSD       -10   /* Connection closed.       */
#define ESPCONN_CONN       -11   /* Not connected.           */

#define ESPCONN_ARG        -12   /* Illegal argument.        */
#define ESPCONN_ISCONN     -15   /* Already connected.       */

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
	ESPCONN_REUSEADDR = 1,
	ESPCONN_END
};

typedef struct _comon_pkt{
	void *pcb;
	int remote_port;
	uint8 remote_ip[4];
	uint8 *ptrbuf;
	uint16 cntr;
	uint16 write_len;
	uint16 write_total;
	sint8  err;
	uint32 timeout;
	uint32 recv_check;
	enum espconn_option espconn_opt;
	os_timer_t ptimer;
}comon_pkt;

typedef struct _espconn_msg{
	struct espconn *pespconn;
	comon_pkt pcommon;
	uint8 count_opt;
	void *preverse;
	void *pssl;
	struct _espconn_msg *pnext;
}espconn_msg;

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
 * FunctionName : espconn_tcp_set_max_con
 * Description  : set the number of simulatenously active TCP connections
 * Parameters   : num -- total number
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_tcp_set_max_con(uint8 num);
/******************************************************************************
 * FunctionName : espconn_tcp_get_max_con_allow
 * Description  : get the count of simulatenously active connections on the server
 * Parameters   : espconn -- espconn to get the count
 * Returns      : result
*******************************************************************************/

extern sint8 espconn_tcp_get_max_con_allow(struct espconn *espconn);

/******************************************************************************
 * FunctionName : espconn_tcp_set_max_con_allow
 * Description  : set the count of simulatenously active connections on the server
 * Parameters   : espconn -- espconn to set the count
 * Returns      : result
*******************************************************************************/

extern sint8 espconn_tcp_set_max_con_allow(struct espconn *espconn, uint8 num);

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
 * FunctionName : espconn_set_opt
 * Description  : access port value for client so that we don't end up bouncing
 *                all connections at the same time .
 * Parameters   : none
 * Returns      : access port value
*******************************************************************************/
extern sint8 espconn_set_opt(struct espconn *espconn, uint8 opt);

/******************************************************************************
 * FunctionName : espconn_gethostbyname
 * Description  : Resolve a hostname (string) into an IP address.
 * Parameters   : pespconn -- espconn to resolve a hostname
 *                hostname -- the hostname that is to be queried
 *                addr -- pointer to a ip_addr_t where to store the address if 
 *                        it is already cached in the dns_table (only valid if
 *                        ESPCONN_OK is returned!)
 *                found -- a callback function to be called on success, failure
 *                         or timeout (only if ERR_INPROGRESS is returned!)
 * Returns      : err_t return code
 *                - ESPCONN_OK if hostname is a valid IP address string or the host
 *                  name is already in the local names table.
 *                - ESPCONN_INPROGRESS enqueue a request to be sent to the DNS server
 *                  for resolution if no errors are present.
 *                - ESPCONN_ARG: dns client not initialized or invalid hostname
*******************************************************************************/

extern sint8 espconn_gethostbyname(struct espconn *pespconn, const char *name, ip_addr_t *addr, dns_found_callback found);

/******************************************************************************
 * FunctionName : espconn_igmp_join
 * Description  : join a multicast group
 * Parameters   : host_ip -- the ip address of udp server
 * 				  multicast_ip -- multicast ip given by user
 * Returns      : none
*******************************************************************************/
extern sint8 espconn_igmp_join(ip_addr_t *host_ip, ip_addr_t *multicast_ip);

/******************************************************************************
 * FunctionName : espconn_igmp_leave
 * Description  : leave a multicast group
 * Parameters   : host_ip -- the ip address of udp server
 * 				  multicast_ip -- multicast ip given by user
 * Returns      : none
*******************************************************************************/
extern sint8 espconn_igmp_leave(ip_addr_t *host_ip, ip_addr_t *multicast_ip);

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

