/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: espconn.c
 *
 * Description: espconn interface for user
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
#include "ets_sys.h"
#include "os_type.h"
//#include "os.h"
#include "lwip/mem.h"

#include "lwip/app/espconn_tcp.h"
#include "lwip/app/espconn_udp.h"
#include "lwip/app/espconn.h"

#include "user_interface.h"

espconn_msg *plink_active = NULL;
espconn_msg *pserver_list = NULL;
remot_info premot[5];
uint32 link_timer = 0;

/******************************************************************************
 * FunctionName : espconn_copy_partial
 * Description  : reconnect with host
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
espconn_copy_partial(struct espconn *pesp_dest, struct espconn *pesp_source)
{
	pesp_dest->type = pesp_source->type;
	pesp_dest->state = pesp_source->state;
	if (pesp_source->type == ESPCONN_TCP){
		pesp_dest->proto.tcp->remote_port = pesp_source->proto.tcp->remote_port;
		pesp_dest->proto.tcp->local_port = pesp_source->proto.tcp->local_port;
		os_memcpy(pesp_dest->proto.tcp->remote_ip, pesp_source->proto.tcp->remote_ip, 4);
		os_memcpy(pesp_dest->proto.tcp->local_ip, pesp_source->proto.tcp->local_ip, 4);
		pesp_dest->proto.tcp->connect_callback = pesp_source->proto.tcp->connect_callback;
		pesp_dest->proto.tcp->reconnect_callback = pesp_source->proto.tcp->reconnect_callback;
		pesp_dest->proto.tcp->disconnect_callback = pesp_source->proto.tcp->disconnect_callback;
	} else {
		pesp_dest->proto.udp->remote_port = pesp_source->proto.udp->remote_port;
		pesp_dest->proto.udp->local_port = pesp_source->proto.udp->local_port;
		os_memcpy(pesp_dest->proto.udp->remote_ip, pesp_source->proto.udp->remote_ip, 4);
		os_memcpy(pesp_dest->proto.udp->local_ip, pesp_source->proto.udp->local_ip, 4);
	}
	pesp_dest->recv_callback = pesp_source->recv_callback;
	pesp_dest->sent_callback = pesp_source->sent_callback;
	pesp_dest->link_cnt = pesp_source->link_cnt;
	pesp_dest->reverse = pesp_source->reverse;
}

/******************************************************************************
 * FunctionName : espconn_copy_partial
 * Description  : insert the node to the active connection list
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR espconn_list_creat(espconn_msg **phead, espconn_msg* pinsert)
{
	espconn_msg *plist = NULL;
//	espconn_msg *ptest = NULL;
	if (*phead == NULL)
		*phead = pinsert;
	else {
		plist = *phead;
		while (plist->pnext != NULL) {
			plist = plist->pnext;
		}
		plist->pnext = pinsert;
	}
	pinsert->pnext = NULL;

/*	ptest = *phead;
	while(ptest != NULL){
		os_printf("espconn_list_creat %p\n", ptest);
		ptest = ptest->pnext;
	}*/
}

/******************************************************************************
 * FunctionName : espconn_list_delete
 * Description  : remove the node from the active connection list
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR espconn_list_delete(espconn_msg **phead, espconn_msg* pdelete)
{
	espconn_msg *plist = NULL;
//	espconn_msg *ptest = NULL;
	plist = *phead;
	if (plist == NULL){
		*phead = NULL;
	} else {
		if (plist == pdelete){
			*phead = plist->pnext;
		} else {
			while (plist != NULL) {
				if (plist->pnext == pdelete){
					plist->pnext = pdelete->pnext;
				}
				plist = plist->pnext;
			}
		}
	}
/*	ptest = *phead;
	while(ptest != NULL){
		os_printf("espconn_list_delete %p\n", ptest);
		ptest = ptest->pnext;
	}*/
}

/******************************************************************************
 * FunctionName : espconn_find_connection
 * Description  : Initialize the server: set up a listening PCB and bind it to
 *                the defined port
 * Parameters   : espconn -- the espconn used to build server
 * Returns      : none
 *******************************************************************************/
bool ICACHE_FLASH_ATTR espconn_find_connection(struct espconn *pespconn, espconn_msg **pnode)
{
	espconn_msg *plist = NULL;
	struct ip_addr ip_remot;
	struct ip_addr ip_list;
	plist = plink_active;

	while(plist != NULL){
		if (pespconn == plist->pespconn){
			*pnode = plist;
			return true;
		} else {
			IP4_ADDR(&ip_remot, pespconn->proto.tcp->remote_ip[0], pespconn->proto.tcp->remote_ip[1],
					pespconn->proto.tcp->remote_ip[2], pespconn->proto.tcp->remote_ip[3]);

			IP4_ADDR(&ip_list, plist->pcommon.remote_ip[0], plist->pcommon.remote_ip[1],
					plist->pcommon.remote_ip[2], plist->pcommon.remote_ip[3]);
			if ((ip_list.addr == ip_remot.addr) && (pespconn->proto.tcp->remote_port == plist->pcommon.remote_port)){
				*pnode = plist;
				return true;
			}
		}
		plist = plist ->pnext;
	}
	return false;
}

/******************************************************************************
 * FunctionName : espconn_connect
 * Description  : The function given as the connect
 * Parameters   : espconn -- the espconn used to listen the connection
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_connect(struct espconn *espconn)
{
	struct ip_addr ipaddr;
	struct ip_info ipinfo;
	uint8 connect_status = 0;
	sint8 value = ESPCONN_OK;
	espconn_msg *plist = NULL;

    if (espconn == NULL) {
        return ESPCONN_ARG;
    } else if (espconn ->type != ESPCONN_TCP)
    	return ESPCONN_ARG;

    if (wifi_get_opmode() == ESPCONN_STA){
    	wifi_get_ip_info(STA_NETIF,&ipinfo);
    	if (ipinfo.ip.addr == 0){
   	 		return ESPCONN_RTE;
   	 	}
    } else if(wifi_get_opmode() == ESPCONN_AP){
    	wifi_get_ip_info(AP_NETIF,&ipinfo);
    	if (ipinfo.ip.addr == 0){
    		return ESPCONN_RTE;
    	}
    } else if(wifi_get_opmode() == ESPCONN_AP_STA){
    	IP4_ADDR(&ipaddr, espconn->proto.tcp->remote_ip[0],
    	    	    		espconn->proto.tcp->remote_ip[1],
    	    	    		espconn->proto.tcp->remote_ip[2],
    	    	    		espconn->proto.tcp->remote_ip[3]);
    	ipaddr.addr <<= 8;
    	wifi_get_ip_info(AP_NETIF,&ipinfo);
    	ipinfo.ip.addr <<= 8;
    	espconn_printf("softap_addr = %x, remote_addr = %x\n", ipinfo.ip.addr, ipaddr.addr);

    	if (ipaddr.addr != ipinfo.ip.addr){
    		connect_status = wifi_station_get_connect_status();
			if (connect_status == STATION_GOT_IP){
				wifi_get_ip_info(STA_NETIF,&ipinfo);
				if (ipinfo.ip.addr == 0)
					return ESPCONN_RTE;
			} else {
				return connect_status;
			}
    	}
    }

    for (plist = plink_active; plist != NULL; plist = plist->pnext){
    	if (plist->pespconn->type == ESPCONN_TCP){
    		if (espconn->proto.tcp->local_port == plist->pespconn->proto.tcp->local_port){
    			return ESPCONN_ISCONN;
    		}
    	}
    }

    value = espconn_tcp_client(espconn);

    return value;
}

/******************************************************************************
 * FunctionName : espconn_create
 * Description  : sent data for client or server
 * Parameters   : espconn -- espconn to the data transmission
 * Returns      : result
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_create(struct espconn *espconn)
{
	sint8 value = ESPCONN_OK;
	espconn_msg *plist = NULL;

	if (espconn == NULL) {
		return ESPCONN_ARG;
	} else if (espconn ->type != ESPCONN_UDP){
		return ESPCONN_ARG;
	}

	for (plist = plink_active; plist != NULL; plist = plist->pnext){
		if (plist->pespconn->type == ESPCONN_UDP){
			if (espconn->proto.udp->local_port == plist->pespconn->proto.udp->local_port){
				return ESPCONN_ISCONN;
			}
		}
	}

	value = espconn_udp_server(espconn);

	return value;
}

/******************************************************************************
 * FunctionName : espconn_sent
 * Description  : sent data for client or server
 * Parameters   : espconn -- espconn to set for client or server
 *                psent -- data to send
 *                length -- length of data to send
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_sent(struct espconn *espconn, uint8 *psent, uint16 length)
{
	espconn_msg *pnode = NULL;
	bool value = false;
    if (espconn == NULL) {
        return ESPCONN_ARG;
    }
    espconn ->state = ESPCONN_WRITE;
    value = espconn_find_connection(espconn, &pnode);
    switch (espconn ->type) {
        case ESPCONN_TCP:
            // if (value && (pnode->pcommon.write_len == pnode->pcommon.write_total)){
        	if (value && (pnode->pcommon.cntr == 0)){
           		espconn_tcp_sent(pnode, psent, length);
            }else
            	return ESPCONN_ARG;
            break;

        case ESPCONN_UDP: {
        	if (value)
        		espconn_udp_sent(pnode, psent, length);
        	else
        		return ESPCONN_ARG;
            break;
        }

        default :
            break;
    }
    return ESPCONN_OK;
}

/******************************************************************************
 * FunctionName : espconn_tcp_get_max_con
 * Description  : get the number of simulatenously active TCP connections
 * Parameters   : espconn -- espconn to set the connect callback
 * Returns      : none
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR espconn_tcp_get_max_con(void)
{
	uint8 tcp_num = 0;

	tcp_num = MEMP_NUM_TCP_PCB;

	return tcp_num;
}

/******************************************************************************
 * FunctionName : espconn_tcp_set_max_con
 * Description  : set the number of simulatenously active TCP connections
 * Parameters   : espconn -- espconn to set the connect callback
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR espconn_tcp_set_max_con(uint8 num)
{
	if (num == 0)
		return ESPCONN_ARG;

	MEMP_NUM_TCP_PCB = num;
	return ESPCONN_OK;
}

/******************************************************************************
 * FunctionName : espconn_tcp_get_max_con_allow
 * Description  : get the count of simulatenously active connections on the server
 * Parameters   : espconn -- espconn to get the count
 * Returns      : result
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR espconn_tcp_get_max_con_allow(struct espconn *espconn)
{
	espconn_msg *pget_msg = NULL;
	if ((espconn == NULL) || (espconn->type == ESPCONN_UDP))
		return ESPCONN_ARG;

	pget_msg = pserver_list;
	while (pget_msg != NULL){
		if (pget_msg->pespconn == espconn){
			return pget_msg->count_opt;
		}
		pget_msg = pget_msg->pnext;
	}
	return ESPCONN_ARG;
}

/******************************************************************************
 * FunctionName : espconn_tcp_set_max_con_allow
 * Description  : set the count of simulatenously active connections on the server
 * Parameters   : espconn -- espconn to set the count
 * Returns      : result
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR espconn_tcp_set_max_con_allow(struct espconn *espconn, uint8 num)
{
	espconn_msg *pset_msg = NULL;
	if ((espconn == NULL) || (num > MEMP_NUM_TCP_PCB) || (espconn->type == ESPCONN_UDP))
		return ESPCONN_ARG;

	pset_msg = pserver_list;
	while (pset_msg != NULL){
		if (pset_msg->pespconn == espconn){
			pset_msg->count_opt = num;
			return ESPCONN_OK;
		}
		pset_msg = pset_msg->pnext;
	}
	return ESPCONN_ARG;
}

/******************************************************************************
 * FunctionName : espconn_regist_sentcb
 * Description  : Used to specify the function that should be called when data
 *                has been successfully delivered to the remote host.
 * Parameters   : espconn -- espconn to set the sent callback
 *                sent_cb -- sent callback function to call for this espconn
 *                when data is successfully sent
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_regist_sentcb(struct espconn *espconn, espconn_sent_callback sent_cb)
{
    if (espconn == NULL) {
    	return ESPCONN_ARG;
    }

    espconn ->sent_callback = sent_cb;
    return ESPCONN_OK;
}

/******************************************************************************
 * FunctionName : espconn_regist_connectcb
 * Description  : used to specify the function that should be called when
 *                connects to host.
 * Parameters   : espconn -- espconn to set the connect callback
 *                connect_cb -- connected callback function to call when connected
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_regist_connectcb(struct espconn *espconn, espconn_connect_callback connect_cb)
{
    if (espconn == NULL) {
    	return ESPCONN_ARG;
    }

    espconn->proto.tcp->connect_callback = connect_cb;
    return ESPCONN_OK;
}

/******************************************************************************
 * FunctionName : espconn_regist_recvcb
 * Description  : used to specify the function that should be called when recv
 *                data from host.
 * Parameters   : espconn -- espconn to set the recv callback
 *                recv_cb -- recv callback function to call when recv data
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_regist_recvcb(struct espconn *espconn, espconn_recv_callback recv_cb)
{
    if (espconn == NULL) {
    	return ESPCONN_ARG;
    }

    espconn ->recv_callback = recv_cb;
    return ESPCONN_OK;
}

/******************************************************************************
 * FunctionName : espconn_regist_reconcb
 * Description  : used to specify the function that should be called when connection
 *                because of err disconnect.
 * Parameters   : espconn -- espconn to set the err callback
 *                recon_cb -- err callback function to call when err
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_regist_reconcb(struct espconn *espconn, espconn_reconnect_callback recon_cb)
{
    if (espconn == NULL) {
    	return ESPCONN_ARG;
    }

    espconn ->proto.tcp->reconnect_callback = recon_cb;
    return ESPCONN_OK;
}

/******************************************************************************
 * FunctionName : espconn_regist_disconcb
 * Description  : used to specify the function that should be called when disconnect
 * Parameters   : espconn -- espconn to set the err callback
 *                discon_cb -- err callback function to call when err
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_regist_disconcb(struct espconn *espconn, espconn_connect_callback discon_cb)
{
    if (espconn == NULL) {
    	return ESPCONN_ARG;
    }

    espconn ->proto.tcp->disconnect_callback = discon_cb;
    return ESPCONN_OK;
}

/******************************************************************************
 * FunctionName : espconn_get_connection_info
 * Description  : used to specify the function that should be called when disconnect
 * Parameters   : espconn -- espconn to set the err callback
 *                discon_cb -- err callback function to call when err
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_get_connection_info(struct espconn *pespconn, remot_info **pcon_info, uint8 typeflags)
{
	espconn_msg *plist = NULL;

	if (pespconn == NULL)
		return ESPCONN_ARG;

	os_memset(premot, 0, sizeof(premot));
	pespconn->link_cnt = 0;
	plist = plink_active;
	switch (pespconn->type){
		case ESPCONN_TCP:
			while(plist != NULL){
				if ((plist->pespconn->type == ESPCONN_TCP) && (plist->preverse == pespconn)){
					switch (typeflags){
						case ESPCONN_SSL:
							if (plist->pssl != NULL){
								premot[pespconn->link_cnt].state = plist->pespconn->state;
								premot[pespconn->link_cnt].remote_port = plist->pcommon.remote_port;
								os_memcpy(premot[pespconn->link_cnt].remote_ip, plist->pcommon.remote_ip, 4);
								pespconn->link_cnt ++;
							}
							break;
						case ESPCONN_NORM:
							if (plist->pssl == NULL){
								premot[pespconn->link_cnt].state = plist->pespconn->state;
								premot[pespconn->link_cnt].remote_port = plist->pcommon.remote_port;
								os_memcpy(premot[pespconn->link_cnt].remote_ip, plist->pcommon.remote_ip, 4);
								pespconn->link_cnt ++;
							}
							break;
						default:
							break;
					}
				}
				plist = plist->pnext;
			}

			break;
		case ESPCONN_UDP:
			while(plist != NULL){
				if (plist->pespconn->type == ESPCONN_UDP){
					premot[pespconn->link_cnt].state = plist->pespconn->state;
					premot[pespconn->link_cnt].remote_port = plist->pcommon.remote_port;
					os_memcpy(premot[pespconn->link_cnt].remote_ip, plist->pcommon.remote_ip, 4);
					pespconn->link_cnt ++;
				}
				plist = plist->pnext;
			}
			break;
		default:
			break;
	}
	*pcon_info = premot;
	return ESPCONN_OK;
}

/******************************************************************************
 * FunctionName : espconn_accept
 * Description  : The function given as the listen
 * Parameters   : espconn -- the espconn used to listen the connection
 * Returns      :
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_accept(struct espconn *espconn)
{
	sint8 value = ESPCONN_OK;
	espconn_msg *plist = NULL;

    if (espconn == NULL) {
        return ESPCONN_ARG;
    } else if (espconn ->type != ESPCONN_TCP)
    	return ESPCONN_ARG;

    for (plist = plink_active; plist != NULL; plist = plist->pnext){
    	if (plist->pespconn->type == ESPCONN_TCP){
    		if (espconn->proto.tcp->local_port == plist->pespconn->proto.tcp->local_port){
    			return ESPCONN_ISCONN;
    		}
    	}
    }
    value = espconn_tcp_server(espconn);

    return value;
}

/******************************************************************************
 * FunctionName : espconn_regist_time
 * Description  : used to specify the time that should be called when don't recv data
 * Parameters   : espconn -- the espconn used to the connection
 * 				  interval -- the timer when don't recv data
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR espconn_regist_time(struct espconn *espconn, uint32 interval, uint8 type_flag)
{
	espconn_msg *pnode = NULL;
	bool value = false;
	if ((espconn == NULL) || (type_flag > 0x01))
		return ESPCONN_ARG;

	if (type_flag == 0x01){
		value = espconn_find_connection(espconn, &pnode);
		if (value){
			pnode->pcommon.timeout = interval;
			return ESPCONN_OK;
		} else
			return ESPCONN_ARG;
	} else {
		link_timer = interval;
		os_printf("espconn_regist_time %d\n", link_timer);
		return ESPCONN_OK;
	}
}

/******************************************************************************
 * FunctionName : espconn_disconnect
 * Description  : disconnect with host
 * Parameters   : espconn -- the espconn used to disconnect the connection
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_disconnect(struct espconn *espconn)
{
	espconn_msg *pnode = NULL;
	bool value = false;

    if (espconn == NULL) {
        return ESPCONN_ARG;;
    } else if (espconn ->type != ESPCONN_TCP)
    	return ESPCONN_ARG;

    value = espconn_find_connection(espconn, &pnode);

    if (value){
    	espconn_tcp_disconnect(pnode);
    	return ESPCONN_OK;
    } else
    	return ESPCONN_ARG;
}

/******************************************************************************
 * FunctionName : espconn_set_opt
 * Description  : access port value for client so that we don't end up bouncing
 *                all connections at the same time .
 * Parameters   : none
 * Returns      : access port value
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_set_opt(struct espconn *espconn, uint8 opt)
{
	espconn_msg *pnode = NULL;
	bool value = false;

	if (espconn == NULL || opt > ESPCONN_END) {
		return ESPCONN_ARG;;
	} else if (espconn->type != ESPCONN_TCP)
		return ESPCONN_ARG;

	value = espconn_find_connection(espconn, &pnode);
	if (value) {
		pnode->pcommon.espconn_opt = opt;
		return ESPCONN_OK;
	} else
		return ESPCONN_ARG;
}

/******************************************************************************
 * FunctionName : espconn_delete
 * Description  : disconnect with host
 * Parameters   : espconn -- the espconn used to disconnect the connection
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_delete(struct espconn *espconn)
{
	espconn_msg *pnode = NULL;
	bool value = false;

    if (espconn == NULL) {
        return ESPCONN_ARG;
    } else if (espconn ->type != ESPCONN_UDP)
    	return espconn_tcp_delete(espconn);

    value = espconn_find_connection(espconn, &pnode);

    if (value){
    	espconn_udp_disconnect(pnode);
    	return ESPCONN_OK;
    } else
    	return ESPCONN_ARG;
}

/******************************************************************************
 * FunctionName : espconn_port
 * Description  : access port value for client so that we don't end up bouncing
 *                all connections at the same time .
 * Parameters   : none
 * Returns      : access port value
*******************************************************************************/
uint32 espconn_port(void)
{
    uint32 port = 0;
    static uint32 randnum = 0;

    do {
        port = system_get_time();

        if (port < 0) {
            port = os_random() - port;
        }

        port %= 0xc350;

        if (port < 0x400) {
            port += 0x400;
        }

    } while (port == randnum);

    randnum = port;

    return port;
}

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
err_t ICACHE_FLASH_ATTR
espconn_gethostbyname(struct espconn *pespconn, const char *hostname, ip_addr_t *addr, dns_found_callback found)
{
    return dns_gethostbyname(hostname, addr, found, pespconn);
}

sint8 espconn_recv_hold(struct espconn *pespconn) {
  espconn_msg *pnode = NULL;

  if (pespconn == NULL) {
    return ESPCONN_ARG;
  }
  pespconn->state = ESPCONN_WRITE;
  if (!espconn_find_connection(pespconn, &pnode)) {
      return ESPCONN_ARG;
  }

  espconn_tcp_hold(pnode);
  return ESPCONN_OK;
}

sint8 espconn_recv_unhold(struct espconn *pespconn) {
  espconn_msg *pnode = NULL;

  if (pespconn == NULL) {
    return ESPCONN_ARG;
  }
  pespconn->state = ESPCONN_WRITE;
  if (!espconn_find_connection(pespconn, &pnode)) {
      return ESPCONN_ARG;
  }

  espconn_tcp_unhold(pnode);
  return ESPCONN_OK;
}
