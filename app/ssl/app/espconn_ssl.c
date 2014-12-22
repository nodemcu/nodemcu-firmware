/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: espconn_ssl.c
 *
 * Description: ssl encrypt interface
 *
 * Modification history:
 *     2014/3/31, v1.0 create this file.
*******************************************************************************/

#include "lwip/netif.h"
#include "netif/etharp.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"
#include "lwip/init.h"
#include "lwip/tcp_impl.h"

#include "ssl/ssl_os_port.h"

#include "ssl/app/espconn_ssl.h"
#include "ets_sys.h"
#include "os_type.h"
//#include "os.h"
#include "lwip/app/espconn.h"

struct pbuf *psslpbuf = NULL;
extern espconn_msg *plink_active;

static err_t espconn_ssl_crecv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
static err_t espconn_ssl_srecv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);

static void espconn_ssl_sclose(void *arg, struct tcp_pcb *pcb);
static void espconn_ssl_cclose(void *arg, struct tcp_pcb *pcb);

/////////////////////////////common function///////////////////////////////////
/******************************************************************************
 * FunctionName : display_session_id
 * Description  : Display what session id we have.
 * Parameters   :
 * Returns      :
*******************************************************************************/
static void ICACHE_FLASH_ATTR display_session_id(SSL *ssl)
{
    int i;
    const uint8_t *session_id = ssl_get_session_id(ssl);
    int sess_id_size = ssl_get_session_id_size(ssl);

    if (sess_id_size > 0) {
        ssl_printf("-----BEGIN SSL SESSION PARAMETERS-----\n");

        for (i = 0; i < sess_id_size; i++) {
            ssl_printf("%02x", session_id[i]);
        }

        ssl_printf("\n-----END SSL SESSION PARAMETERS-----\n");
        //TTY_FLUSH();
    }
}

/******************************************************************************
 * FunctionName : display_cipher
 * Description  : Display what cipher we are using
 * Parameters   :
 * Returns      :
*******************************************************************************/
static void ICACHE_FLASH_ATTR display_cipher(SSL *ssl)
{
    ssl_printf("CIPHER is ");

    switch (ssl_get_cipher_id(ssl)) {
        case SSL_AES128_SHA:
            ssl_printf("AES128-SHA");
            break;

        case SSL_AES256_SHA:
            ssl_printf("AES256-SHA");
            break;

        case SSL_RC4_128_SHA:
            ssl_printf("RC4-SHA");
            break;

        case SSL_RC4_128_MD5:
            ssl_printf("RC4-MD5");
            break;

        default:
            ssl_printf("Unknown - %d", ssl_get_cipher_id(ssl));
            break;
    }

    ssl_printf("\n");
    //TTY_FLUSH();
}

/******************************************************************************
 * FunctionName : espconn_ssl_reconnect
 * Description  : reconnect with host
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
static void ICACHE_FLASH_ATTR
espconn_ssl_reconnect(void *arg)
{
	espconn_msg *pssl_recon = arg;
    struct espconn *espconn = NULL;
    ssl_msg *pssl = NULL;
    sint8 ssl_reerr = 0;
    if (pssl_recon != NULL) {
    	espconn = pssl_recon->preverse;
    	if (pssl_recon->pespconn != NULL){
    		if (espconn != NULL){
    			/*espconn_copy_partial(espconn, pssl_recon->pespconn);
    			if (pssl_recon->pespconn->proto.tcp != NULL){
    				os_free(pssl_recon->pespconn->proto.tcp);
    				pssl_recon->pespconn->proto.tcp = NULL;
    			}
    			os_free(pssl_recon->pespconn);
    			pssl_recon->pespconn = NULL;*/
    			espconn = pssl_recon->preverse;
    		} else {
    			espconn = pssl_recon->pespconn;
    		}
    	}
        pssl = pssl_recon->pssl;
        ssl_reerr = pssl_recon->pcommon.err;
        if (pssl != NULL) {
            if (pssl->ssl) {
                ssl_free(pssl->ssl);
            }

            if (pssl->ssl_ctx) {
                ssl_ctx_free(pssl->ssl_ctx);
            }

            os_free(pssl);
            pssl = NULL;
            pssl_recon->pssl = pssl;
        }
        os_free(pssl_recon);
        pssl_recon = NULL;
        if (espconn ->proto.tcp->reconnect_callback != NULL) {
            espconn ->proto.tcp->reconnect_callback(espconn, ssl_reerr);
        }
    } else {
        ssl_printf("espconn_ssl_reconnect err\n");
    }
}

/******************************************************************************
 * FunctionName : espconn_ssl_dissuccessful
 * Description  : as
 * Parameters   :
 * Returns      :
*******************************************************************************/
static void ICACHE_FLASH_ATTR
espconn_ssl_dissuccessful(void *arg)
{
	espconn_msg *pdiscon = arg;
    struct espconn *espconn = NULL;
    struct tcp_pcb *pcb = NULL;
    ssl_msg *pssl = NULL;
    if (pdiscon != NULL) {
    	espconn = pdiscon->preverse;
    	if (pdiscon->pespconn != NULL){
    		if (espconn != NULL){
    			/*espconn_copy_partial(espconn, pdiscon->pespconn);
    			if (pdiscon->pespconn->proto.tcp != NULL){
    				os_free(pdiscon->pespconn->proto.tcp);
    				pdiscon->pespconn->proto.tcp = NULL;
    			}
    			os_free(pdiscon->pespconn);
    			pdiscon->pespconn = NULL;*/
    			espconn = pdiscon->preverse;
    		} else{
    			espconn = pdiscon->pespconn;
    		}
    		pcb = pdiscon->pcommon.pcb;
    		tcp_arg(pcb, NULL);
    		tcp_err(pcb, NULL);
    	}

        pssl = pdiscon->pssl;

        if (pssl != NULL) {
            if (pssl->ssl) {
                ssl_free(pssl->ssl);
            }

            if (pssl->ssl_ctx) {
                ssl_ctx_free(pssl->ssl_ctx);
            }

            os_free(pssl);
            pssl = NULL;
            pdiscon->pssl = pssl;
        }

        os_free(pdiscon);
        pdiscon = NULL;
        if (espconn ->proto.tcp->disconnect_callback != NULL) {
            espconn ->proto.tcp->disconnect_callback(espconn);
        }
    } else {
        espconn_printf("espconn_ssl_dissuccessful err\n");
    }
}

/******************************************************************************
 * FunctionName : espconn_ssl_write
 * Description  : sent data for client or server
 * Parameters   : void *arg -- client or server to send
 *                uint8* psent -- Data to send
 *                uint16 length -- Length of data to send
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
espconn_ssl_sent(void *arg, uint8 *psent, uint16 length)
{
	espconn_msg *pssl_sent = arg;
    struct tcp_pcb *pcb = NULL;
    ssl_msg *pssl = NULL;
    u16_t len = 0;
    int res = 0;

    ssl_printf("espconn_ssl_sent pcb %p psent %p length %d\n", arg, psent, length);

    if (pssl_sent == NULL || psent == NULL || length == 0) {
        return;
    }

    pcb = pssl_sent->pcommon.pcb;
	pssl = pssl_sent->pssl;
    if (RT_MAX_PLAIN_LENGTH < length) {
        len = RT_MAX_PLAIN_LENGTH;
    } else {
        len = length;
    }

    if (pssl != NULL) {
        if (pssl->ssl != NULL) {
            pssl->ssl->SslClient_pcb = pcb;
            res = ssl_write(pssl->ssl, psent, len);
            pssl_sent->pcommon.ptrbuf = psent + len;
            pssl_sent->pcommon.cntr = length - len;
        }
    }
}

/******************************************************************************
 * FunctionName : espconn_sent_packet
 * Description  : sent data for client or server
 * Parameters   : void *arg -- client or server to send
 *                uint8* psent -- Data to send
 *                uint16 length -- Length of data to send
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
espconn_sent_packet(struct tcp_pcb *pcb, uint8 *psent, uint16 length)
{
	err_t err = 0;
	u16_t len = 0;
	if (pcb == NULL || psent == NULL || length == 0) {
	   return;
	}

	if (tcp_sndbuf(pcb) < length) {
	   len = tcp_sndbuf(pcb);
	} else {
	   len = length;
	}

	if (len > (2 * pcb->mss)) {
	   len = 2 * pcb->mss;
	}

	do {
	   err = tcp_write(pcb, psent, len, 0);
	   if (err == ERR_MEM) {
	      len /= 2;
	   }
	} while (err == ERR_MEM && len > 1);

	if (err == ERR_OK) {
	   err = tcp_output(pcb);
	}
}

////////////////////////////////client function////////////////////////////////
/******************************************************************************
 * FunctionName : espconn_ssl_cclose_cb
 * Description  : as
 * Parameters   :
 * Returns      :
*******************************************************************************/
static void ICACHE_FLASH_ATTR
espconn_ssl_cclose_cb(void *arg)
{
    static uint16 timecount = 0;
    espconn_msg *pcclose_cb = arg;

    if (pcclose_cb == NULL) {
        return;
    }

    struct tcp_pcb *pcb = pcclose_cb->pcommon.pcb;

    ssl_printf("espconn_ssl_cclose_cb %d %d\n", pcb->state, pcb->nrtx);

    if (pcb->state == TIME_WAIT || pcb->state == CLOSED) {
    	pcclose_cb->pespconn ->state = ESPCONN_CLOSE;
    	/*remove the node from the client's active connection list*/
    	espconn_list_delete(&plink_active, pcclose_cb);
        espconn_ssl_dissuccessful((void *)pcclose_cb);
    } else {
    	os_timer_arm(&pcclose_cb->pcommon.ptimer, TCP_FAST_INTERVAL, 0);
    }
}

/******************************************************************************
 * FunctionName : espconn_sslclient_close
 * Description  : The connection shall be actively closed.
 * Parameters   : pcb -- Additional argument to pass to the callback function
 *                pcb -- the pcb to close
 * Returns      : none
*******************************************************************************/
static void ICACHE_FLASH_ATTR
espconn_ssl_cclose(void *arg, struct tcp_pcb *pcb)
{
    espconn_msg *pcclose = arg;

    os_timer_disarm(&pcclose->pcommon.ptimer);
    os_timer_setfn(&pcclose->pcommon.ptimer, espconn_ssl_cclose_cb, pcclose);
    os_timer_arm(&pcclose->pcommon.ptimer, TCP_FAST_INTERVAL, 0);
    tcp_recv(pcb, NULL);
    pcclose->pcommon.err = tcp_close(pcb);
    ssl_printf("espconn_ssl_cclose %d\n", pcclose->pcommon.err);

    if (pcclose->pcommon.err != ERR_OK) {
        /* closing failed, try again later */
        tcp_recv(pcb, espconn_ssl_crecv);
    } else {
        tcp_sent(pcb, NULL);
        tcp_poll(pcb, NULL, 0);
    }
}

/******************************************************************************
 * FunctionName : espconn_sslclient_sent
 * Description  : Data has been sent and acknowledged by the remote host.
 *                This means that more data can be sent.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb for which data has been acknowledged
 *                len -- The amount of bytes acknowledged
 * Returns      : ERR_OK: try to send some data by calling tcp_output
 *                ERR_ABRT: if you have called tcp_abort from within the function!
*******************************************************************************/
static err_t ICACHE_FLASH_ATTR
espconn_ssl_csent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	espconn_msg *psent = arg;
    ssl_msg *pssl = psent->pssl;
    psent->pcommon.pcb = pcb;
    if (pssl->quiet == true) {
    	int pkt_size = pssl->ssl->bm_index + SSL_RECORD_SIZE;
    	u16_t max_len = 2 * pcb->mss;
    	pssl->pkt_length += len;
    	ssl_printf("espconn_ssl_csent %d %d %d\n", len, pssl->pkt_length, pkt_size);
    	if (pssl->pkt_length == pkt_size){
    		pssl->ssl->bm_index = 0;
    		pssl->pkt_length = 0;
    		if (psent->pcommon.cntr == 0) {
    			psent->pespconn->state = ESPCONN_CONNECT;
    			if (psent->pespconn->sent_callback != NULL) {
    				psent->pespconn->sent_callback(psent->pespconn);
    			}
    		} else {
    			espconn_ssl_sent(psent, psent->pcommon.ptrbuf, psent->pcommon.cntr);
    		}
    	} else {
    		if (len == max_len){
    			espconn_sent_packet(pcb, &pssl->ssl->bm_all_data[pssl->pkt_length], pkt_size - pssl->pkt_length);
    		}
    	}

    } else {
    	ssl_printf("espconn_ssl_csent %p %p %d\n", pcb, pssl->ssl->bm_all_data, len);
    }

    return ERR_OK;
}

/******************************************************************************
 * FunctionName : espconn_sslclient_recv
 * Description  : Data has been received on this pcb.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb which received data
 *                p -- The received data (or NULL when the connection has been closed!)
 *                err -- An error code if there has been an error receiving
 * Returns      : ERR_ABRT: if you have called tcp_abort from within the function!
*******************************************************************************/
static err_t ICACHE_FLASH_ATTR
espconn_ssl_crecv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    u16_t ret = 0;
    espconn_msg *precv = arg;
    ssl_msg *pssl = precv->pssl;
    ssl_printf("espconn_ssl_crecv %d %p %p\n", __LINE__, pssl->ssl, p);

    if (p != NULL) {
        tcp_recved(pcb, p ->tot_len);

        if (pssl->ssl == NULL) {
            pbuf_free(p);
        } else {
            pssl->ssl->ssl_pbuf = p;

            if (ssl_handshake_status(pssl->ssl) != SSL_OK) {
                ret = ssl_read(pssl->ssl, NULL);
                pbuf_free(p);
                if (ret != SSL_OK){
                    os_printf("client handshake failed\n");
                    espconn_ssl_cclose(arg, pcb);
                }
            }

            if (ssl_handshake_status(pssl->ssl) == SSL_OK) {
                if (!pssl->quiet) {
                	ssl_printf("client handshake need size %d\n", system_get_free_heap_size());
                    const char *common_name = ssl_get_cert_dn(pssl->ssl,
                                              SSL_X509_CERT_COMMON_NAME);

                    if (common_name) {
                        ssl_printf("Common Name:\t\t\t%s\n", common_name);
                    }

                    display_session_id(pssl->ssl);
                    display_cipher(pssl->ssl);
                    pssl->quiet = true;
                    os_printf("client handshake ok!\n");
                    REG_CLR_BIT(0x3ff00014, BIT(0));
                    os_update_cpu_frequency(80);
                    precv->pespconn->state = ESPCONN_CONNECT;
                    precv->pcommon.pcb = pcb;
                    pbuf_free(p);

                    if (precv->pespconn->proto.tcp->connect_callback != NULL) {
                    	precv->pespconn->proto.tcp->connect_callback(precv->pespconn);
                    }
                } else {
                    uint8_t *read_buf = NULL;
                    ret = ssl_read(pssl->ssl, &read_buf);
                    precv->pespconn->state = ESPCONN_READ;
                    precv->pcommon.pcb = pcb;
                    pbuf_free(p);

                    if (precv->pespconn->recv_callback != NULL && read_buf != NULL) {
                    	precv->pespconn->recv_callback(precv->pespconn, read_buf, ret);
                    }

                    precv->pespconn->state = ESPCONN_CONNECT;
                }
            }
        }

    }

    if (err == ERR_OK && p == NULL) {
        espconn_ssl_cclose(precv, pcb);
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
espconn_ssl_cerr(void *arg, err_t err)
{
	espconn_msg *pssl_cerr = arg;
	struct tcp_pcb *pcb = NULL;
    LWIP_UNUSED_ARG(err);

    if (pssl_cerr != NULL) {
    	os_timer_disarm(&pssl_cerr->pcommon.ptimer);
        pcb = pssl_cerr->pcommon.pcb;
        pssl_cerr->pespconn->state = ESPCONN_CLOSE;
        espconn_printf("espconn_ssl_cerr %d %d %d\n", pcb->state, pcb->nrtx, err);

        /*remove the node from the client's active connection list*/
        espconn_list_delete(&plink_active, pssl_cerr);


        if (err == ERR_ABRT) {
        	switch (pcb->state) {
        		case SYN_SENT:
        			if (pcb->nrtx == TCP_SYNMAXRTX) {
        				pssl_cerr->pcommon.err = ESPCONN_CONN;
        			} else {
        				pssl_cerr->pcommon.err = err;
        			}

        			break;

        		case ESTABLISHED:
        			if (pcb->nrtx == TCP_MAXRTX) {
        				pssl_cerr->pcommon.err = ESPCONN_TIMEOUT;
        			} else {
        				pssl_cerr->pcommon.err = err;
        			}

        			break;

        		case FIN_WAIT_1:
        			if (pcb->nrtx == TCP_MAXRTX) {
        				pssl_cerr->pcommon.err = ESPCONN_CLSD;
        			} else {
        				pssl_cerr->pcommon.err = err;
        			}
        			break;
        		case FIN_WAIT_2:
        			pssl_cerr->pcommon.err = ESPCONN_CLSD;
        			break;
        		case CLOSED:
        			pssl_cerr->pcommon.err = ESPCONN_CONN;
        			break;
        		default :
        			break;
        	}
        } else {
        	pssl_cerr->pcommon.err = err;
        }

        os_timer_setfn(&pssl_cerr->pcommon.ptimer, espconn_ssl_reconnect, pssl_cerr);
        os_timer_arm(&pssl_cerr->pcommon.ptimer, 10, 0);
    }
}

#if 0
/******************************************************************************
 * FunctionName : espconn_ssl_cpoll
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
espconn_ssl_cpoll(void *arg, struct tcp_pcb *pcb)
{
    ssl_printf("espconn_ssl_cpoll %p %d\n", pcb, pcb->state);
    struct espconn *espconn = arg;

    if (arg == NULL) {
        tcp_abandon(pcb, 0);
        tcp_poll(pcb, NULL, 0);
        return ERR_ABRT;
    }

    if (pcb ->state == ESTABLISHED) {
    	espconn->recv_check ++;
    	if (espconn ->recv_check == 0x05){
			//tcp_poll(pcb, espconn_ssl_cpoll, 0);
			espconn->recv_check = 0;
			espconn_ssl_cclose(arg, pcb);
    	}
    } else {
        //tcp_poll(pcb, espconn_ssl_cpoll, 0);
        espconn_ssl_cclose(arg, pcb);
    }

    return ERR_OK;
}
#endif
/******************************************************************************
 * FunctionName : espconn_sslclient_connect
 * Description  : A new incoming connection has been connected.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                tpcb -- The connection pcb which is connected
 *                err -- An unused error code, always ERR_OK currently
 * Returns      : connection result
*******************************************************************************/
static err_t ICACHE_FLASH_ATTR
espconn_ssl_connect(void *arg, struct tcp_pcb *tpcb, err_t err)
{
	espconn_msg *pconnect = arg;
    ssl_msg *pssl = NULL;
    uint32_t options;
    options = SSL_SERVER_VERIFY_LATER | SSL_DISPLAY_CERTS | SSL_NO_DEFAULT_KEY;
    ssl_printf("espconn_ssl_connect %p %p %p %d\n", tpcb, arg, pespconn->psecure, system_get_free_heap_size());

    //if (pespconn->psecure != NULL){
    //    return ERR_ISCONN;
    //}

    pconnect->pcommon.pcb = tpcb;
    pssl = (ssl_msg *)os_zalloc(sizeof(ssl_msg));
    pconnect->pssl = pssl;

    if (pssl == NULL) {
        return ERR_MEM;
    }

    REG_SET_BIT(0x3ff00014, BIT(0));
    os_update_cpu_frequency(160);
    os_printf("client handshake start.\n");
    pssl->quiet = false;
    pssl->ssl_ctx = ssl_ctx_new(options, SSL_DEFAULT_CLNT_SESS);

    if (pssl->ssl_ctx == NULL) {
        return ERR_MEM;
    }

    ssl_printf("espconn_ssl_client ssl_ctx %p\n", pssl->ssl_ctx);
    pssl->ssl = SSLClient_new(pssl->ssl_ctx, tpcb, NULL, 0);

    if (pssl->ssl == NULL) {
        return ERR_MEM;
    }

    tcp_arg(tpcb, arg);
    tcp_sent(tpcb, espconn_ssl_csent);
    tcp_recv(tpcb, espconn_ssl_crecv);
    //tcp_poll(tpcb, espconn_ssl_cpoll, 6);
    return ERR_OK;
}

/******************************************************************************
 * FunctionName : espconn_ssl_disconnect
 * Description  : A new incoming connection has been disconnected.
 * Parameters   : espconn -- the espconn used to disconnect with host
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR espconn_ssl_disconnect(espconn_msg *pdis)
{
    if (pdis != NULL) {
    	if (pdis->preverse == NULL)
    		espconn_ssl_cclose(pdis, pdis->pcommon.pcb);
    	else
    		espconn_ssl_sclose(pdis, pdis->pcommon.pcb);
    } else {
        ssl_printf("espconn_ssl_disconnect err.\n");
    }
}

/******************************************************************************
 * FunctionName : espconn_ssl_client
 * Description  : Initialize the client: set up a connect PCB and bind it to
 *                the defined port
 * Parameters   : espconn -- the espconn used to build client
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_ssl_client(struct espconn *espconn)
{
    struct tcp_pcb *pcb;
    struct ip_addr ipaddr;
    espconn_msg *pclient = NULL;
    pclient = plink_active;
    while(pclient != NULL){
    	if (pclient->pssl != NULL)
    		return ESPCONN_ISCONN;

    	pclient = pclient->pnext;
    }
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
        espconn ->state = ESPCONN_NONE;
        os_free(pclient);
        pclient = NULL;
        return ESPCONN_MEM;
    } else {
    	/*insert the node to the active connection list*/
    	espconn_list_creat(&plink_active, pclient);
    	tcp_arg(pcb, (void *)pclient);
    	tcp_err(pcb, espconn_ssl_cerr);
    	pclient->preverse = NULL;
    	pclient->pespconn = espconn;
    	pclient->pespconn->state = ESPCONN_WAIT;
    	pclient->pcommon.pcb = pcb;
        tcp_bind(pcb, IP_ADDR_ANY, pclient->pespconn->proto.tcp->local_port);
        pclient->pcommon.err = tcp_connect(pcb, &ipaddr, pclient->pespconn->proto.tcp->remote_port, espconn_ssl_connect);
        return ESPCONN_OK;
    }
}

/////////////////////////////server's function/////////////////////////////////
/******************************************************************************
 * FunctionName : espconn_ssl_sclose_cb
 * Description  : as
 * Parameters   :
 * Returns      :
*******************************************************************************/
static void ICACHE_FLASH_ATTR
espconn_ssl_sclose_cb(void *arg)
{
    static uint16 timecount = 0;
    espconn_msg *psclose_cb = arg;

    if (psclose_cb == NULL) {
        return;
    }

    struct tcp_pcb *pcb = psclose_cb->pcommon.pcb;

    ssl_printf("espconn_ssl_sclose_cb %d %d\n", pcb->state, pcb->nrtx);

    if (pcb->state == CLOSED || pcb->state == TIME_WAIT) {
    	psclose_cb ->pespconn ->state = ESPCONN_CLOSE;
    	psclose_cb->pespconn->link_cnt --;
    	/*remove the node from the server's active connection list*/
    	espconn_list_delete(&plink_active, psclose_cb);
        espconn_ssl_dissuccessful((void *)psclose_cb);
    } else {
    	os_timer_arm(&psclose_cb->pcommon.ptimer, TCP_FAST_INTERVAL, 0);
    }
}

/******************************************************************************
 * FunctionName : espconn_sslclient_close
 * Description  : The connection shall be actively closed.
 * Parameters   : pcb -- Additional argument to pass to the callback function
 *                pcb -- the pcb to close
 * Returns      : none
*******************************************************************************/
static void ICACHE_FLASH_ATTR
espconn_ssl_sclose(void *arg, struct tcp_pcb *pcb)
{
    espconn_msg *psclose = arg;

    os_timer_disarm(&psclose->pcommon.ptimer);
    os_timer_setfn(&psclose->pcommon.ptimer, espconn_ssl_sclose_cb, psclose);
    os_timer_arm(&psclose->pcommon.ptimer, TCP_FAST_INTERVAL, 0);
    tcp_recv(pcb, NULL);
    psclose->pcommon.err = tcp_close(pcb);

    if (psclose->pcommon.err != ERR_OK) {
        /* closing failed, try again later */
        tcp_recv(pcb, espconn_ssl_srecv);
    } else {
        tcp_sent(pcb, NULL);
        tcp_poll(pcb, NULL, 0);
    }
}


/******************************************************************************
 * FunctionName : espconn_sslclient_sent
 * Description  : Data has been sent and acknowledged by the remote host.
 *                This means that more data can be sent.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb for which data has been acknowledged
 *                len -- The amount of bytes acknowledged
 * Returns      : ERR_OK: try to send some data by calling tcp_output
 *                ERR_ABRT: if you have called tcp_abort from within the function!
*******************************************************************************/
static err_t ICACHE_FLASH_ATTR
espconn_ssl_ssent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	espconn_msg *psent = arg;
    ssl_msg *pssl = psent->pssl;
    psent->pcommon.pcb = pcb;
    psent->pcommon.recv_check = 0;
    if (ssl_handshake_status(pssl->ssl) == SSL_OK) {
        if (!pssl->quiet) {
           ssl_printf("espconn_ssl_ssent %p %d\n",pcb, system_get_free_heap_size());
           const char *common_name = ssl_get_cert_dn(pssl->ssl, SSL_X509_CERT_COMMON_NAME);

           if (common_name) {
              ssl_printf("Common Name:\t\t\t%s\n", common_name);
           }

           display_session_id(pssl->ssl);
           display_cipher(pssl->ssl);
           pssl->quiet = true;
           os_printf("server handshake ok!\n");
           REG_CLR_BIT(0x3ff00014, BIT(0));
           os_update_cpu_frequency(80);
           psent->pespconn->state = ESPCONN_CONNECT;

           if (psent->pespconn->proto.tcp->connect_callback != NULL) {
        	   psent->pespconn->proto.tcp->connect_callback(psent->pespconn);
           }
       } else {

    	   int pkt_size = pssl->ssl->bm_index + SSL_RECORD_SIZE;
    	   u16_t max_len = 2 * pcb->mss;
    	   pssl->pkt_length += len;
    	   ssl_printf("espconn_ssl_ssent %d %d %d\n", len, pssl->pkt_length, pkt_size);
    	   if (pssl->pkt_length == pkt_size){
    		   pssl->ssl->bm_index = 0;
    		   pssl->pkt_length = 0;
    	       if (psent->pcommon.cntr == 0) {
    	    	   psent->pespconn->state = ESPCONN_CONNECT;
    	    	   if (psent->pespconn->sent_callback != NULL) {
    	    		   psent->pespconn->sent_callback(psent->pespconn);
    	    	   }
    	       } else {
    	    	   espconn_ssl_sent(psent, psent->pcommon.ptrbuf, psent->pcommon.cntr);
    	       }
    	   } else {
    		   if (len == max_len){
    			   espconn_sent_packet(pcb, &pssl->ssl->bm_all_data[pssl->pkt_length], pkt_size - pssl->pkt_length);
    		   }
    	   }
       }
    } else {
    	ssl_printf("espconn_ssl_ssent %p %p %d\n",pcb, pssl->ssl->bm_all_data, len);
    }

    return ERR_OK;
}

/******************************************************************************
 * FunctionName : espconn_sslclient_recv
 * Description  : Data has been received on this pcb.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb which received data
 *                p -- The received data (or NULL when the connection has been closed!)
 *                err -- An error code if there has been an error receiving
 * Returns      : ERR_ABRT: if you have called tcp_abort from within the function!
*******************************************************************************/
static err_t ICACHE_FLASH_ATTR
espconn_ssl_srecv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    u16_t ret = 0;
    espconn_msg *precv = arg;
    ssl_msg *pssl = precv->pssl;
    ssl_printf("espconn_ssl_srecv %d %p %p\n", __LINE__, pcb, p);

    if (p != NULL) {
        tcp_recved(pcb, p ->tot_len);
        precv->pcommon.recv_check = 0;
        if (pssl->ssl == NULL) {
            pbuf_free(p);
        } else {
            pssl->ssl->ssl_pbuf = p;

            if (ssl_handshake_status(pssl->ssl) != SSL_OK) {
                ret = ssl_read(pssl->ssl, NULL);
                pbuf_free(p);
                if (ret != SSL_OK){
                	os_printf("server handshake failed.\n");
                	espconn_ssl_sclose(arg, pcb);
                }
            } else {
                    uint8_t *read_buf = NULL;
                    ret = ssl_read(pssl->ssl, &read_buf);
                    precv->pespconn->state = ESPCONN_READ;
                    precv->pcommon.pcb = pcb;
                    pbuf_free(p);

                    if (precv->pespconn->recv_callback != NULL && read_buf != NULL) {
                    	precv->pespconn->recv_callback(precv->pespconn, read_buf, ret);
                    }

                    precv->pespconn->state = ESPCONN_CONNECT;
                }

        }

    }

    if (err == ERR_OK && p == NULL) {
        espconn_ssl_sclose(precv, pcb);
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
espconn_ssl_spoll(void *arg, struct tcp_pcb *pcb)
{
    ssl_printf("espconn_ssl_spoll %p %d\n", pcb, pcb->state);
    espconn_msg *pspoll = arg;
    if (arg == NULL) {
        tcp_abandon(pcb, 0);
        tcp_poll(pcb, NULL, 0);
        return ERR_ABRT;
    }

    if (pcb ->state == ESTABLISHED) {
    	pspoll ->pcommon.recv_check ++;
    	if (pspoll ->pcommon.recv_check == pspoll ->pcommon.timeout){
			tcp_poll(pcb, NULL, 0);
    		pspoll ->pcommon.recv_check = 0;
			espconn_ssl_sclose(arg, pcb);
    	}
    } else {
    	tcp_poll(pcb, NULL, 0);
        espconn_ssl_sclose(arg, pcb);
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
espconn_ssl_serr(void *arg, err_t err)
{
	espconn_msg *pserr = arg;
	struct tcp_pcb *pcb = NULL;
    LWIP_UNUSED_ARG(err);

    if (pserr != NULL) {
    	os_timer_disarm(&pserr->pcommon.ptimer);
        pcb = pserr->pcommon.pcb;
        pserr->pespconn->state = ESPCONN_CLOSE;

        /*remove the node from the server's active connection list*/
        espconn_list_delete(&plink_active, pserr);

        if (err == ERR_ABRT) {
        	switch (pcb->state) {
        		case SYN_RCVD:
        			if (pcb->nrtx == TCP_SYNMAXRTX) {
        				pserr->pcommon.err = ESPCONN_CONN;
        			} else {
        				pserr->pcommon.err = err;
        			}

        			break;

        		case ESTABLISHED:
        			if (pcb->nrtx == TCP_MAXRTX) {
        				pserr->pcommon.err = ESPCONN_TIMEOUT;
        			} else {
        				pserr->pcommon.err = err;
        			}

        			break;

        		case CLOSE_WAIT:
        			if (pcb->nrtx == TCP_MAXRTX) {
        				pserr->pcommon.err = ESPCONN_CLSD;
        			} else {
        				pserr->pcommon.err = err;
        			}
        			break;
        		case LAST_ACK:
        			pserr->pcommon.err = ESPCONN_CLSD;
        			break;
        		case CLOSED:
        			pserr->pcommon.err = ESPCONN_CONN;
        			break;
        		default :
        			break;
        	}
        } else {
        	pserr->pcommon.err = err;
        }

        os_timer_setfn(&pserr->pcommon.ptimer, espconn_ssl_reconnect, pserr);
        os_timer_arm(&pserr->pcommon.ptimer, 10, 0);
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
espconn_ssl_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
    struct espconn *espconn = arg;
    ssl_msg *pssl = NULL;
    espconn_msg *paccept = NULL;
    remot_info *pinfo = NULL;
    ssl_printf("espconn_ssl_accept %p %p %p %d\n", pcb, arg, espconn->psecure, system_get_free_heap_size());
    LWIP_UNUSED_ARG(err);

	paccept = (espconn_msg *)os_zalloc(sizeof(espconn_msg));
	tcp_arg(pcb, paccept);
	tcp_err(pcb, espconn_ssl_serr);
	if (paccept == NULL)
		return ERR_MEM;
	/*insert the node to the active connection list*/
	espconn_list_creat(&plink_active, paccept);
	paccept->preverse = espconn;
	paccept->pespconn = espconn;

	paccept->pcommon.timeout = 0x0a;
	paccept->pcommon.pcb = pcb;
	paccept->pcommon.remote_port = pcb->remote_port;
	paccept->pcommon.remote_ip[0] = ip4_addr1_16(&pcb->remote_ip);
	paccept->pcommon.remote_ip[1] = ip4_addr2_16(&pcb->remote_ip);
	paccept->pcommon.remote_ip[2] = ip4_addr3_16(&pcb->remote_ip);
	paccept->pcommon.remote_ip[3] = ip4_addr4_16(&pcb->remote_ip);
	os_memcpy(espconn->proto.tcp->remote_ip, paccept->pcommon.remote_ip, 4);
	espconn->proto.tcp->remote_port = pcb->remote_port;

	espconn_get_connection_info(espconn, &pinfo , ESPCONN_SSL);
	if (espconn->link_cnt == 0x01)
		return ERR_ISCONN;

    pssl = (ssl_msg *)os_zalloc(sizeof(ssl_msg));
    paccept->pssl = pssl;
    if (pssl == NULL) {
        return ERR_MEM;
    }

    REG_SET_BIT(0x3ff00014, BIT(0));
    os_update_cpu_frequency(160);
    os_printf("server handshake start.\n");
    pssl->quiet = false;
    pssl->ssl_ctx = ssl_ctx_new(SSL_DISPLAY_CERTS, SSL_DEFAULT_SVR_SESS);

    if (pssl->ssl_ctx == NULL) {
        ssl_printf("Error: Server context is invalid\n");
        return ERR_MEM;
    }

    ssl_printf("Server context %p\n", pssl->ssl_ctx);
    pssl->ssl = sslserver_new(pssl->ssl_ctx, pcb);

    if (pssl->ssl == NULL) {
        ssl_printf("Error: Server ssl connection is invalid\n");
        return ERR_MEM;

    }

    tcp_sent(pcb, espconn_ssl_ssent);
    tcp_recv(pcb, espconn_ssl_srecv);
    tcp_poll(pcb, espconn_ssl_spoll, 2);
    return ERR_OK;
}

/******************************************************************************
 * FunctionName : espconn_ssl_server
 * Description  : as
 * Parameters   :
 * Returns      :
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR espconn_ssl_server(struct espconn *espconn)
{
    struct tcp_pcb *pcb;

    pcb = tcp_new();
    if (pcb == NULL) {
        espconn ->state = ESPCONN_NONE;
        return ESPCONN_MEM;
    } else {
        tcp_bind(pcb, IP_ADDR_ANY, espconn->proto.tcp->local_port);
        pcb = tcp_listen(pcb);
        if (pcb != NULL) {
            espconn ->state = ESPCONN_LISTEN;
            tcp_arg(pcb, (void *)espconn);
            tcp_accept(pcb, espconn_ssl_accept);
            return ESPCONN_OK;
        } else {
            espconn ->state = ESPCONN_NONE;
            return ESPCONN_MEM;
        }
    }
}


