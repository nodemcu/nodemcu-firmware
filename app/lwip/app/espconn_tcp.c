/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: espconn_tcp.c
 *
 * Description: tcp proto interface
 *
 * Modification history:
 *     2014/3/31, v1.0 create this file.
*******************************************************************************/

#include "lwip/netif.h"
#include "lwip/inet.h"
#include "netif/etharp.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"
#include "lwip/init.h"
#include "lwip/tcp_impl.h"
#include "lwip/memp.h"

#include "ets_sys.h"
#include "os_type.h"
//#include "os.h"
#include "lwip/mem.h"
#include "lwip/app/espconn_tcp.h"

extern espconn_msg *plink_active;
extern uint32 link_timer;
extern espconn_msg *pserver_list;

static err_t
espconn_client_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
static void
espconn_client_close(void *arg, struct tcp_pcb *pcb);

static err_t
espconn_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
static void
espconn_server_close(void *arg, struct tcp_pcb *pcb);

///////////////////////////////common function/////////////////////////////////

/******************************************************************************
 * FunctionName : espconn_tcp_reconnect
 * Description  : reconnect with host
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
static void ICACHE_FLASH_ATTR
espconn_tcp_reconnect(void *arg)
{
	espconn_msg *precon_cb = arg;
	sint8 re_err = 0;
	if (precon_cb != NULL) {
		struct espconn *espconn = precon_cb->preverse;
		re_err = precon_cb->pcommon.err;
		if (precon_cb->pespconn != NULL){
			if (espconn != NULL){
				if (precon_cb->pespconn->proto.tcp != NULL){
					espconn_copy_partial(espconn, precon_cb->pespconn);
					espconn_printf("server: %d.%d.%d.%d : %d reconnection\n", espconn->proto.tcp->remote_ip[0],
							espconn->proto.tcp->remote_ip[1],espconn->proto.tcp->remote_ip[2],
							espconn->proto.tcp->remote_ip[3],espconn->proto.tcp->remote_port);
					os_free(precon_cb->pespconn->proto.tcp);
					precon_cb->pespconn->proto.tcp = NULL;
				}
				os_free(precon_cb->pespconn);
				precon_cb->pespconn = NULL;
			} else {
				espconn = precon_cb->pespconn;
				espconn_printf("client: %d.%d.%d.%d : %d reconnection\n", espconn->proto.tcp->local_ip[0],
										espconn->proto.tcp->local_ip[1],espconn->proto.tcp->local_ip[2],
										espconn->proto.tcp->local_ip[3],espconn->proto.tcp->local_port);
			}
		}
		os_free(precon_cb);
		precon_cb = NULL;

		if (espconn->proto.tcp->reconnect_callback != NULL) {
			espconn->proto.tcp->reconnect_callback(espconn, re_err);
		}
	} else {
		espconn_printf("espconn_tcp_reconnect err\n");
	}
}

/******************************************************************************
 * FunctionName : espconn_tcp_disconnect
 * Description  : disconnect with host
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
static void ICACHE_FLASH_ATTR
espconn_tcp_disconnect_successful(void *arg)
{
	espconn_msg *pdiscon_cb = arg;
	sint8 dis_err = 0;
	if (pdiscon_cb != NULL) {
		struct espconn *espconn = pdiscon_cb->preverse;

		dis_err = pdiscon_cb->pcommon.err;
		if (pdiscon_cb->pespconn != NULL){
			struct tcp_pcb *pcb = NULL;
			if (espconn != NULL){
				if (pdiscon_cb->pespconn->proto.tcp != NULL){
					espconn_copy_partial(espconn, pdiscon_cb->pespconn);
					espconn_printf("server: %d.%d.%d.%d : %d disconnect\n", espconn->proto.tcp->remote_ip[0],
							espconn->proto.tcp->remote_ip[1],espconn->proto.tcp->remote_ip[2],
							espconn->proto.tcp->remote_ip[3],espconn->proto.tcp->remote_port);
					os_free(pdiscon_cb->pespconn->proto.tcp);
					pdiscon_cb->pespconn->proto.tcp = NULL;
				}
				os_free(pdiscon_cb->pespconn);
				pdiscon_cb->pespconn = NULL;
			} else {
				espconn = pdiscon_cb->pespconn;
				espconn_printf("client: %d.%d.%d.%d : %d disconnect\n", espconn->proto.tcp->local_ip[0],
						espconn->proto.tcp->local_ip[1],espconn->proto.tcp->local_ip[2],
						espconn->proto.tcp->local_ip[3],espconn->proto.tcp->local_port);
			}
			pcb = pdiscon_cb->pcommon.pcb;
			tcp_arg(pcb, NULL);
			tcp_err(pcb, NULL);
			/*delete TIME_WAIT State pcb after 2MSL time,for not all data received by application.*/
			if (pdiscon_cb->pcommon.espconn_opt == ESPCONN_REUSEADDR){
				if (pcb->state == TIME_WAIT){
					tcp_pcb_remove(&tcp_tw_pcbs,pcb);
					memp_free(MEMP_TCP_PCB,pcb);
				}
			}
		}
		os_free(pdiscon_cb);
		pdiscon_cb = NULL;

		if (espconn->proto.tcp->disconnect_callback != NULL) {
			espconn->proto.tcp->disconnect_callback(espconn);
		}
	} else {
		espconn_printf("espconn_tcp_disconnect err\n");
	}
}

/******************************************************************************
 * FunctionName : espconn_tcp_sent
 * Description  : sent data for client or server
 * Parameters   : void *arg -- client or server to send
 *                uint8* psent -- Data to send
 *                uint16 length -- Length of data to send
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
espconn_tcp_sent(void *arg, uint8 *psent, uint16 length)
{
	espconn_msg *ptcp_sent = arg;
    struct tcp_pcb *pcb = NULL;
    err_t err = 0;
    u16_t len = 0;
    u8_t data_to_send = false;

    espconn_printf("espconn_tcp_sent ptcp_sent %p psent %p length %d\n", ptcp_sent, psent, length);

    if (ptcp_sent == NULL || psent == NULL || length == 0) {
        return;
    }

    pcb = ptcp_sent->pcommon.pcb;
    if (tcp_sndbuf(pcb) < length) {
        len = tcp_sndbuf(pcb);
    } else {
        len = length;
        LWIP_ASSERT("length did not fit into uint16!", (len == length));
    }

    if (len > (2*pcb->mss)) {
        len = 2*pcb->mss;
    }

    do {
    	espconn_printf("espconn_tcp_sent writing %d bytes %p\n", len, pcb);
        err = tcp_write(pcb, psent, len, 0);

        if (err == ERR_MEM) {
            len /= 2;
        }
    } while (err == ERR_MEM && len > 1);

    if (err == ERR_OK) {
        data_to_send = true;
        ptcp_sent->pcommon.ptrbuf = psent + len;
        ptcp_sent->pcommon.cntr = length - len;
        ptcp_sent->pcommon.write_len += len;
        espconn_printf("espconn_tcp_sent sending %d bytes, remain %d\n", len, ptcp_sent->pcommon.cntr);
    }

    if (data_to_send == true) {
        err = tcp_output(pcb);
    } else {
    	ptcp_sent->pespconn ->state = ESPCONN_CLOSE;
    }
}

/******************************************************************************
 * FunctionName : espconn_close
 * Description  : The connection has been successfully closed.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR espconn_tcp_disconnect(espconn_msg *pdiscon)
{
	if (pdiscon != NULL){
		if (pdiscon->preverse != NULL)
			espconn_server_close(pdiscon, pdiscon->pcommon.pcb);
		else
			espconn_client_close(pdiscon, pdiscon->pcommon.pcb);
	} else{
		espconn_printf("espconn_server_disconnect err.\n");
	}
}

///////////////////////////////client function/////////////////////////////////
/******************************************************************************
 * FunctionName : espconn_close
 * Description  : The connection has been successfully closed.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
static void ICACHE_FLASH_ATTR
espconn_cclose_cb(void *arg)
{
	espconn_msg *pclose_cb = arg;

    if (pclose_cb == NULL) {
        return;
    }

    struct tcp_pcb *pcb = pclose_cb->pcommon.pcb;

    espconn_printf("espconn_close %d %d\n", pcb->state, pcb->nrtx);

    if (pcb->state == TIME_WAIT || pcb->state == CLOSED) {
    	pclose_cb ->pespconn ->state = ESPCONN_CLOSE;
    	/*remove the node from the client's active connection list*/
    	espconn_list_delete(&plink_active, pclose_cb);
    	espconn_tcp_disconnect_successful((void*)pclose_cb);
    } else {
		os_timer_arm(&pclose_cb->pcommon.ptimer, TCP_FAST_INTERVAL, 0);
    }
}

/******************************************************************************
 * FunctionName : espconn_client_close
 * Description  : The connection shall be actively closed.
 * Parameters   : pcb -- Additional argument to pass to the callback function
 *                pcb -- the pcb to close
 * Returns      : none
*******************************************************************************/
static void ICACHE_FLASH_ATTR
espconn_client_close(void *arg, struct tcp_pcb *pcb)
{
    err_t err;
    espconn_msg *pclose = arg;

	os_timer_disarm(&pclose->pcommon.ptimer);

	tcp_recv(pcb, NULL);
	err = tcp_close(pcb);
	os_timer_setfn(&pclose->pcommon.ptimer, espconn_cclose_cb, pclose);
	os_timer_arm(&pclose->pcommon.ptimer, TCP_FAST_INTERVAL, 0);

    if (err != ERR_OK) {
        /* closing failed, try again later */
        tcp_recv(pcb, espconn_client_recv);
    } else {
        /* closing succeeded */
        tcp_sent(pcb, NULL);
    }
}

/******************************************************************************
 * FunctionName : espconn_client_recv
 * Description  : Data has been received on this pcb.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb which received data
 *                p -- The received data (or NULL when the connection has been closed!)
 *                err -- An error code if there has been an error receiving
 * Returns      : ERR_ABRT: if you have called tcp_abort from within the function!
*******************************************************************************/
static err_t ICACHE_FLASH_ATTR
espconn_client_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	espconn_msg *precv_cb = arg;

	tcp_arg(pcb, arg);

    if (p != NULL) {
        tcp_recved(pcb, p ->tot_len);
    }

    if (err == ERR_OK && p != NULL) {
    	char *pdata = NULL;
    	u16_t length = 0;
        pdata = (char *)os_zalloc(p ->tot_len + 1);
        length = pbuf_copy_partial(p, pdata, p ->tot_len, 0);
        pbuf_free(p);

        if (length != 0) {
        	precv_cb->pespconn ->state = ESPCONN_READ;
        	precv_cb->pcommon.pcb = pcb;
            if (precv_cb->pespconn->recv_callback != NULL) {
            	precv_cb->pespconn->recv_callback(precv_cb->pespconn, pdata, length);
            }
            precv_cb->pespconn ->state = ESPCONN_CONNECT;
        }

        os_free(pdata);
        pdata = NULL;
    }

    if (err == ERR_OK && p == NULL) {
        espconn_client_close(precv_cb, pcb);
    }

    return ERR_OK;
}

/******************************************************************************
 * FunctionName : espconn_client_sent
 * Description  : Data has been sent and acknowledged by the remote host.
 *                This means that more data can be sent.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb for which data has been acknowledged
 *                len -- The amount of bytes acknowledged
 * Returns      : ERR_OK: try to send some data by calling tcp_output
 *                ERR_ABRT: if you have called tcp_abort from within the function!
*******************************************************************************/
static err_t ICACHE_FLASH_ATTR
espconn_client_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	espconn_msg *psent_cb = arg;

	psent_cb->pcommon.pcb = pcb;
	psent_cb->pcommon.write_total += len;
	espconn_printf("espconn_client_sent sent %d %d\n", len, psent_cb->pcommon.write_total);
	if (psent_cb->pcommon.write_total == psent_cb->pcommon.write_len){
		psent_cb->pcommon.write_total = 0;
		psent_cb->pcommon.write_len = 0;
		if (psent_cb->pcommon.cntr == 0) {
			psent_cb->pespconn->state = ESPCONN_CONNECT;

			if (psent_cb->pespconn->sent_callback != NULL) {
				psent_cb->pespconn->sent_callback(psent_cb->pespconn);
			}
		} else
			espconn_tcp_sent(psent_cb, psent_cb->pcommon.ptrbuf, psent_cb->pcommon.cntr);
	} else {

	}
    return ERR_OK;
}

/******************************************************************************
 * FunctionName : espconn_client_poll
 * Description  : The poll function is called every 2nd second.
 * If there has been no data sent (which resets the retries) in 8 seconds, close.
 * If the last portion of a file has not been sent in 2 seconds, close.
 *
 * This could be increased, but we don't want to waste resources for bad connections.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb for which data has been acknowledged
 * Returns      : ERR_OK: try to send some data by calling tcp_output
 *                ERR_ABRT: if you have called tcp_abort from within the function!
*******************************************************************************/
static err_t ICACHE_FLASH_ATTR
espconn_client_poll(void *arg, struct tcp_pcb *pcb)
{
	espconn_msg *ppoll_cb = arg;
	espconn_printf("espconn_client_poll pcb %p %p %d\n", pcb, arg, pcb->state);
    if (arg == NULL) {
        tcp_abandon(pcb, 0);
        tcp_poll(pcb, NULL, 0);
        return ERR_ABRT;
    }
    ppoll_cb->pcommon.pcb = pcb;
    if (pcb->state == ESTABLISHED) {
    } else {
        tcp_poll(pcb, espconn_client_poll, 0);
        espconn_client_close(ppoll_cb->pespconn, pcb);
    }

    return ERR_OK;
}

/******************************************************************************
 * FunctionName : espconn_client_err
 * Description  : The pcb had an error and is already deallocated.
 *                The argument might still be valid (if != NULL).
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                err -- Error code to indicate why the pcb has been closed
 * Returns      : none
*******************************************************************************/
static void ICACHE_FLASH_ATTR
espconn_client_err(void *arg, err_t err)
{
	espconn_msg *perr_cb = arg;
	struct tcp_pcb *pcb = NULL;
    LWIP_UNUSED_ARG(err);

    if (perr_cb != NULL) {
    	os_timer_disarm(&perr_cb->pcommon.ptimer);
        pcb = perr_cb->pcommon.pcb;
        perr_cb->pespconn->state = ESPCONN_CLOSE;
        espconn_printf("espconn_client_err %d %d %d\n", pcb->state, pcb->nrtx, err);

        /*remove the node from the client's active connection list*/
        espconn_list_delete(&plink_active, perr_cb);

        if (err == ERR_ABRT) {
        	switch (pcb->state) {
					case SYN_SENT:
						if (pcb->nrtx == TCP_SYNMAXRTX) {
							perr_cb->pcommon.err = ESPCONN_CONN;
						} else {
							perr_cb->pcommon.err = err;
						}

						break;

					case ESTABLISHED:
						if (pcb->nrtx == TCP_MAXRTX) {
							perr_cb->pcommon.err = ESPCONN_TIMEOUT;
						} else {
							perr_cb->pcommon.err = err;
						}
						break;

					case FIN_WAIT_1:
						if (pcb->nrtx == TCP_MAXRTX) {
							perr_cb->pcommon.err = ESPCONN_CLSD;
						} else {
							perr_cb->pcommon.err = err;
						}
						break;
					case FIN_WAIT_2:
						perr_cb->pcommon.err = ESPCONN_CLSD;
						break;
					case CLOSED:
						perr_cb->pcommon.err = ESPCONN_CONN;
						break;
				}
			} else {
				perr_cb->pcommon.err = err;
			}
			os_timer_setfn(&perr_cb->pcommon.ptimer,
					(os_timer_func_t *) espconn_tcp_reconnect, perr_cb);
			os_timer_arm(&perr_cb->pcommon.ptimer, 10, 0);
		}
}

/******************************************************************************
 * FunctionName : espconn_client_connect
 * Description  : A new incoming connection has been connected.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                tpcb -- The connection pcb which is connected
 *                err -- An unused error code, always ERR_OK currently
 * Returns      : connection result
*******************************************************************************/
static err_t ICACHE_FLASH_ATTR
espconn_client_connect(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    espconn_msg *pcon = arg;
    espconn_printf("espconn_client_connect pcon %p tpcb %p\n", pcon, tpcb);
    if (err == ERR_OK){
		pcon->pespconn->state = ESPCONN_CONNECT;
		pcon->pcommon.err = err;
		pcon->pcommon.pcb = tpcb;
		tcp_arg(tpcb, (void *)pcon);
		tcp_sent(tpcb, espconn_client_sent);
		tcp_recv(tpcb, espconn_client_recv);

		//tcp_poll(tpcb, espconn_client_poll, 1);
		if (pcon->pespconn->proto.tcp->connect_callback != NULL) {
			pcon->pespconn->proto.tcp->connect_callback(pcon->pespconn);
		}
    } else{
    	os_printf("err in host connected (%s)\n",lwip_strerr(err));
    }
    return err;
}

/******************************************************************************
 * FunctionName : espconn_tcp_client
 * Description  : Initialize the client: set up a connect PCB and bind it to
 *                the defined port
 * Parameters   : espconn -- the espconn used to build client
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_tcp_client(struct espconn *espconn)
{
    struct tcp_pcb *pcb = NULL;
    struct ip_addr ipaddr;
    espconn_msg *pclient = NULL;

	pclient = (espconn_msg *)os_zalloc(sizeof(espconn_msg));
	if (pclient == NULL){
		return ESPCONN_MEM;
 	}

    IP4_ADDR(&ipaddr, espconn->proto.tcp->remote_ip[0],
    		espconn->proto.tcp->remote_ip[1],
    		espconn->proto.tcp->remote_ip[2],
    		espconn->proto.tcp->remote_ip[3]);

    pcb = tcp_new();

    if (pcb == NULL) {
//    	pclient ->pespconn ->state = ESPCONN_NONE;
    	os_free(pclient);
    	pclient = NULL;
        return ESPCONN_MEM;
    } else {
    	/*insert the node to the active connection list*/
    	espconn_list_creat(&plink_active, pclient);
    	tcp_arg(pcb, (void *)pclient);
    	tcp_err(pcb, espconn_client_err);
    	pclient->preverse = NULL;
    	pclient->pespconn = espconn;
    	pclient->pespconn->state = ESPCONN_WAIT;
    	pclient->pcommon.pcb = pcb;
        tcp_bind(pcb, IP_ADDR_ANY, pclient->pespconn->proto.tcp->local_port);
        pclient->pcommon.err = tcp_connect(pcb, &ipaddr,
        		pclient->pespconn->proto.tcp->remote_port, espconn_client_connect);
        return pclient->pcommon.err;
    }
}

///////////////////////////////server function/////////////////////////////////
/******************************************************************************
 * FunctionName : espconn_closed
 * Description  : The connection has been successfully closed.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
static void ICACHE_FLASH_ATTR
espconn_sclose_cb(void *arg)
{
	espconn_msg *psclose_cb = arg;
    if (psclose_cb == NULL) {
        return;
    }

    struct tcp_pcb *pcb = psclose_cb ->pcommon.pcb;
    espconn_printf("espconn_sclose_cb %d %d\n", pcb->state, pcb->nrtx);
    if (pcb->state == CLOSED || pcb->state == TIME_WAIT) {
    	psclose_cb ->pespconn ->state = ESPCONN_CLOSE;
		/*remove the node from the server's active connection list*/
		espconn_list_delete(&plink_active, psclose_cb);
		espconn_tcp_disconnect_successful(psclose_cb);
    } else {
		os_timer_arm(&psclose_cb->pcommon.ptimer, TCP_FAST_INTERVAL, 0);
    }
}

/******************************************************************************
 * FunctionName : espconn_server_close
 * Description  : The connection shall be actively closed.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- the pcb to close
 * Returns      : none
*******************************************************************************/
static void ICACHE_FLASH_ATTR
espconn_server_close(void *arg, struct tcp_pcb *pcb)
{
    err_t err;
    espconn_msg *psclose = arg;

	os_timer_disarm(&psclose->pcommon.ptimer);

    tcp_recv(pcb, NULL);
    err = tcp_close(pcb);
	os_timer_setfn(&psclose->pcommon.ptimer, espconn_sclose_cb, psclose);
	os_timer_arm(&psclose->pcommon.ptimer, TCP_FAST_INTERVAL, 0);

    if (err != ERR_OK) {
        /* closing failed, try again later */
        tcp_recv(pcb, espconn_server_recv);
    } else {
        /* closing succeeded */
        tcp_poll(pcb, NULL, 0);
        tcp_sent(pcb, NULL);
    }
}

/******************************************************************************
 * FunctionName : espconn_server_recv
 * Description  : Data has been received on this pcb.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb which received data
 *                p -- The received data (or NULL when the connection has been closed!)
 *                err -- An error code if there has been an error receiving
 * Returns      : ERR_ABRT: if you have called tcp_abort from within the function!
*******************************************************************************/
static err_t ICACHE_FLASH_ATTR
espconn_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	espconn_msg *precv_cb = arg;

    tcp_arg(pcb, arg);
    espconn_printf("server has application data received: %d\n", system_get_free_heap_size());
    if (p != NULL) {
        tcp_recved(pcb, p->tot_len);
    }

    if (err == ERR_OK && p != NULL) {
    	u8_t *data_ptr = NULL;
    	u32_t data_cntr = 0;
		precv_cb->pcommon.recv_check = 0;
        data_ptr = (u8_t *)os_zalloc(p ->tot_len + 1);
        data_cntr = pbuf_copy_partial(p, data_ptr, p ->tot_len, 0);
        pbuf_free(p);

        if (data_cntr != 0) {
        	precv_cb->pespconn ->state = ESPCONN_READ;
        	precv_cb->pcommon.pcb = pcb;
            if (precv_cb->pespconn->recv_callback != NULL) {
            	precv_cb->pespconn->recv_callback(precv_cb->pespconn, data_ptr, data_cntr);
            }
            precv_cb->pespconn ->state = ESPCONN_CONNECT;
        }

        os_free(data_ptr);
        data_ptr = NULL;
        espconn_printf("server's application data has been processed: %d\n", system_get_free_heap_size());
    } else {
        if (p != NULL) {
            pbuf_free(p);
        }

        espconn_server_close(precv_cb, pcb);
    }

    return ERR_OK;
}

/******************************************************************************
 * FunctionName : espconn_server_sent
 * Description  : Data has been sent and acknowledged by the remote host.
 * This means that more data can be sent.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb for which data has been acknowledged
 *                len -- The amount of bytes acknowledged
 * Returns      : ERR_OK: try to send some data by calling tcp_output
 *                ERR_ABRT: if you have called tcp_abort from within the function!
*******************************************************************************/
static err_t ICACHE_FLASH_ATTR
espconn_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	espconn_msg *psent_cb = arg;

	psent_cb->pcommon.pcb = pcb;
	psent_cb->pcommon.recv_check = 0;
	psent_cb->pcommon.write_total += len;
	espconn_printf("espconn_server_sent sent %d %d\n", len, psent_cb->pcommon.write_total);
	if (psent_cb->pcommon.write_total == psent_cb->pcommon.write_len){
		psent_cb->pcommon.write_total = 0;
		psent_cb->pcommon.write_len = 0;
		if (psent_cb ->pcommon.cntr == 0) {
			psent_cb ->pespconn ->state = ESPCONN_CONNECT;

			if (psent_cb ->pespconn ->sent_callback != NULL) {
				psent_cb ->pespconn ->sent_callback(psent_cb ->pespconn);
			}
		} else
			espconn_tcp_sent(psent_cb, psent_cb ->pcommon.ptrbuf, psent_cb ->pcommon.cntr);
	} else {

	}
    return ERR_OK;
}

/******************************************************************************
 * FunctionName : espconn_server_poll
 * Description  : The poll function is called every 3nd second.
 * If there has been no data sent (which resets the retries) in 3 seconds, close.
 * If the last portion of a file has not been sent in 3 seconds, close.
 *
 * This could be increased, but we don't want to waste resources for bad connections.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb for which data has been acknowledged
 * Returns      : ERR_OK: try to send some data by calling tcp_output
 *                ERR_ABRT: if you have called tcp_abort from within the function!
*******************************************************************************/
static err_t ICACHE_FLASH_ATTR
espconn_server_poll(void *arg, struct tcp_pcb *pcb)
{
	espconn_msg *pspoll_cb = arg;

    if (arg == NULL) {
        tcp_abandon(pcb, 0);
        tcp_poll(pcb, NULL, 0);
        return ERR_ABRT;
    }

    espconn_printf("espconn_server_poll %d %d\n", pspoll_cb->pcommon.recv_check, pcb->state);
    pspoll_cb->pcommon.pcb = pcb;
    if (pcb->state == ESTABLISHED) {
		pspoll_cb->pcommon.recv_check++;
		if (pspoll_cb->pcommon.timeout != 0){
			if (pspoll_cb->pcommon.recv_check == pspoll_cb->pcommon.timeout) {
				pspoll_cb->pcommon.recv_check = 0;
				espconn_server_close(pspoll_cb, pcb);
			}
		} else if (link_timer != 0){
			if (pspoll_cb->pcommon.recv_check == link_timer) {
				pspoll_cb->pcommon.recv_check = 0;
				espconn_server_close(pspoll_cb, pcb);
			}
		} else {
			if (pspoll_cb->pcommon.recv_check == 0x0a) {
				pspoll_cb->pcommon.recv_check = 0;
				espconn_server_close(pspoll_cb, pcb);
			}
		}
    } else {
        espconn_server_close(pspoll_cb, pcb);
    }

    return ERR_OK;
}

/******************************************************************************
 * FunctionName : esponn_server_err
 * Description  : The pcb had an error and is already deallocated.
 *                The argument might still be valid (if != NULL).
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                err -- Error code to indicate why the pcb has been closed
 * Returns      : none
*******************************************************************************/
static void ICACHE_FLASH_ATTR
esponn_server_err(void *arg, err_t err)
{
	espconn_msg *pserr_cb = arg;
	struct tcp_pcb *pcb = NULL;
    if (pserr_cb != NULL) {
    	os_timer_disarm(&pserr_cb->pcommon.ptimer);
    	pcb = pserr_cb->pcommon.pcb;
    	pserr_cb->pespconn->state = ESPCONN_CLOSE;

		/*remove the node from the server's active connection list*/
		espconn_list_delete(&plink_active, pserr_cb);

		if (err == ERR_ABRT) {
			switch (pcb->state) {
				case SYN_RCVD:
					if (pcb->nrtx == TCP_SYNMAXRTX) {
						pserr_cb->pcommon.err = ESPCONN_CONN;
					} else {
						pserr_cb->pcommon.err = err;
					}

					break;

				case ESTABLISHED:
					if (pcb->nrtx == TCP_MAXRTX) {
						pserr_cb->pcommon.err = ESPCONN_TIMEOUT;
					} else {
						pserr_cb->pcommon.err = err;
					}

					break;

				case CLOSE_WAIT:
					if (pcb->nrtx == TCP_MAXRTX) {
						pserr_cb->pcommon.err = ESPCONN_CLSD;
					} else {
						pserr_cb->pcommon.err = err;
					}
					break;
				case LAST_ACK:
					pserr_cb->pcommon.err = ESPCONN_CLSD;
					break;

				case CLOSED:
					pserr_cb->pcommon.err = ESPCONN_CONN;
					break;
				default :
					break;
			}
		} else {
			pserr_cb->pcommon.err = err;
		}

		os_timer_setfn(&pserr_cb->pcommon.ptimer,
				(os_timer_func_t *) espconn_tcp_reconnect, pserr_cb);
		os_timer_arm(&pserr_cb->pcommon.ptimer, 10, 0);
    }
}

/******************************************************************************
 * FunctionName : espconn_tcp_accept
 * Description  : A new incoming connection has been accepted.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb which is accepted
 *                err -- An unused error code, always ERR_OK currently
 * Returns      : acception result
*******************************************************************************/
static err_t ICACHE_FLASH_ATTR
espconn_tcp_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
    struct espconn *espconn = arg;
    espconn_msg *paccept = NULL;
    remot_info *pinfo = NULL;
    LWIP_UNUSED_ARG(err);

    paccept = (espconn_msg *)os_zalloc(sizeof(espconn_msg));
    tcp_arg(pcb, paccept);
	tcp_err(pcb, esponn_server_err);
	if (paccept == NULL)
		return ERR_MEM;
	/*insert the node to the active connection list*/
	espconn_list_creat(&plink_active, paccept);

    paccept->preverse = espconn;
	paccept->pespconn = (struct espconn *)os_zalloc(sizeof(struct espconn));
	if (paccept->pespconn == NULL)
		return ERR_MEM;
	paccept->pespconn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	if (paccept->pespconn->proto.tcp == NULL)
		return ERR_MEM;

	//paccept->pcommon.timeout = 0x0a;
	//link_timer = 0x0a;

	paccept->pcommon.pcb = pcb;

	paccept->pcommon.remote_port = pcb->remote_port;
	paccept->pcommon.remote_ip[0] = ip4_addr1_16(&pcb->remote_ip);
	paccept->pcommon.remote_ip[1] = ip4_addr2_16(&pcb->remote_ip);
	paccept->pcommon.remote_ip[2] = ip4_addr3_16(&pcb->remote_ip);
	paccept->pcommon.remote_ip[3] = ip4_addr4_16(&pcb->remote_ip);

	os_memcpy(espconn->proto.tcp->remote_ip, paccept->pcommon.remote_ip, 4);
	espconn->proto.tcp->remote_port = pcb->remote_port;
	espconn->state = ESPCONN_CONNECT;
	espconn_copy_partial(paccept->pespconn, espconn);
	espconn_get_connection_info(espconn, &pinfo , 0);
	espconn_printf("espconn_tcp_accept link_cnt: %d\n", espconn->link_cnt);
	if (espconn->link_cnt == espconn_tcp_get_max_con_allow(espconn) + 1)
		return ERR_ISCONN;

	tcp_sent(pcb, espconn_server_sent);
	tcp_recv(pcb, espconn_server_recv);
	tcp_poll(pcb, espconn_server_poll, 8); /* every 1 seconds */

	if (paccept->pespconn->proto.tcp->connect_callback != NULL) {
		paccept->pespconn->proto.tcp->connect_callback(paccept->pespconn);
	}

    return ERR_OK;
}

/******************************************************************************
 * FunctionName : espconn_tcp_server
 * Description  : Initialize the server: set up a listening PCB and bind it to
 *                the defined port
 * Parameters   : espconn -- the espconn used to build server
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_tcp_server(struct espconn *espconn)
{
    struct tcp_pcb *pcb = NULL;
    espconn_msg *pserver = NULL;

    pserver = (espconn_msg *)os_zalloc(sizeof(espconn_msg));
    if (pserver == NULL){
    	return ESPCONN_MEM;
    }

    pcb = tcp_new();
    if (pcb == NULL) {
//        espconn ->state = ESPCONN_NONE;
    	os_free(pserver);
    	pserver = NULL;
        return ESPCONN_MEM;
    } else {
        tcp_bind(pcb, IP_ADDR_ANY, espconn->proto.tcp->local_port);
        pcb = tcp_listen(pcb);
        if (pcb != NULL) {
        	/*insert the node to the active connection list*/
        	espconn_list_creat(&pserver_list, pserver);
        	pserver->preverse = pcb;
        	pserver->pespconn = espconn;
        	pserver->count_opt = MEMP_NUM_TCP_PCB;

            espconn ->state = ESPCONN_LISTEN;
            tcp_arg(pcb, (void *)espconn);
//            tcp_err(pcb, esponn_server_err);
            tcp_accept(pcb, espconn_tcp_accept);
            return ESPCONN_OK;
        } else {
//            espconn ->state = ESPCONN_NONE;
        	os_free(pserver);
        	pserver = NULL;
            return ESPCONN_MEM;
        }
    }
}

/******************************************************************************
 * FunctionName : espconn_tcp_delete
 * Description  : delete the server: delete a listening PCB and free it
 * Parameters   : pdeletecon -- the espconn used to delete a server
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR espconn_tcp_delete(struct espconn *pdeletecon)
{
	err_t err;
	remot_info *pinfo = NULL;
	espconn_msg *pdelete_msg = NULL;
	struct tcp_pcb *pcb = NULL;

	if (pdeletecon == NULL)
		return ESPCONN_ARG;

	espconn_get_connection_info(pdeletecon, &pinfo , 0);
	if (pdeletecon->link_cnt != 0)
		return ESPCONN_INPROGRESS;
	else {
		os_printf("espconn_tcp_delete %p\n",pdeletecon);
		pdelete_msg = pserver_list;
		while (pdelete_msg != NULL){
			if (pdelete_msg->pespconn == pdeletecon){
				/*remove the node from the client's active connection list*/
				espconn_list_delete(&pserver_list, pdelete_msg);
				pcb = pdelete_msg->preverse;
				os_printf("espconn_tcp_delete %d\n",pcb->state);
				err = tcp_close(pcb);
				os_free(pdelete_msg);
				pdelete_msg = NULL;
				break;
			}
			pdelete_msg = pdelete_msg->pnext;
		}
		if (err == ERR_OK)
			return err;
		else
			return ESPCONN_ARG;
	}
}

void espconn_tcp_hold(void *arg) {
    espconn_msg *ptcp_sent = arg;
    struct tcp_pcb *pcb = NULL;
    pcb = ptcp_sent->pcommon.pcb;

    pcb->hold = 1;
}

void espconn_tcp_unhold(void *arg) {
    espconn_msg *ptcp_sent = arg;
    struct tcp_pcb *pcb = NULL;
    pcb = ptcp_sent->pcommon.pcb;

    pcb->hold = 0;
}
