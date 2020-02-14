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

#include "lwip/netif.h"
#include "lwip/inet.h"
#include "netif/etharp.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"
#include "lwip/init.h"
#include "ets_sys.h"
#include "os_type.h"

#include "lauxlib.h"

#ifdef MEMLEAK_DEBUG
static const char mem_debug_file[] ICACHE_RODATA_ATTR = __FILE__;
#endif

#if !defined(ESPCONN_MBEDTLS)

#include "sys/espconn_mbedtls.h"

struct ssl_options ssl_client_options = {SSL_BUFFER_SIZE, 0, false, 0, false, LUA_NOREF, LUA_NOREF};

/******************************************************************************
 * FunctionName : espconn_encry_connect
 * Description  : The function given as the connect
 * Parameters   : espconn -- the espconn used to listen the connection
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_secure_connect(struct espconn *espconn)
{
	struct ip_addr ipaddr;
	struct ip_info ipinfo;
	uint8 connect_status = 0;
	uint16 current_size = 0;
	if (espconn == NULL || espconn ->type != ESPCONN_TCP)
		return ESPCONN_ARG;

	if (wifi_get_opmode() == ESPCONN_STA){
		wifi_get_ip_info(STA_NETIF, &ipinfo);
		if (ipinfo.ip.addr == 0) {
			return ESPCONN_RTE;
		}
	} else if (wifi_get_opmode() == ESPCONN_AP) {
		wifi_get_ip_info(AP_NETIF, &ipinfo);
		if (ipinfo.ip.addr == 0) {
			return ESPCONN_RTE;
		}
	} else if (wifi_get_opmode() == ESPCONN_AP_STA) {
		IP4_ADDR(&ipaddr, espconn->proto.tcp->remote_ip[0],
				espconn->proto.tcp->remote_ip[1],
				espconn->proto.tcp->remote_ip[2],
				espconn->proto.tcp->remote_ip[3]);
		ipaddr.addr <<= 8;
		wifi_get_ip_info(AP_NETIF, &ipinfo);
		ipinfo.ip.addr <<= 8;
		espconn_printf("softap_addr = %x, remote_addr = %x\n", ipinfo.ip.addr, ipaddr.addr);

		if (ipaddr.addr != ipinfo.ip.addr) {
			connect_status = wifi_station_get_connect_status();
			if (connect_status == STATION_GOT_IP) {
				wifi_get_ip_info(STA_NETIF, &ipinfo);
				if (ipinfo.ip.addr == 0)
					return ESPCONN_RTE;
			} else if (connect_status == STATION_IDLE) {
				return ESPCONN_RTE;
			} else {
				return connect_status;
			}
		}
	}
	current_size = SSL_BUFFER_SIZE;
	current_size += ESPCONN_SECURE_DEFAULT_HEAP;
//	ssl_printf("heap_size %d %d\n", system_get_free_heap_size(), current_size);
	if (system_get_free_heap_size() <= current_size)
		return ESPCONN_MEM;

	return espconn_ssl_client(espconn);
}

/******************************************************************************
 * FunctionName : espconn_encry_disconnect
 * Description  : The function given as the disconnect
 * Parameters   : espconn -- the espconn used to listen the connection
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_secure_disconnect(struct espconn *espconn)
{
	espconn_msg *pnode = NULL;
	bool value = false;
	if (espconn == NULL)
		return ESPCONN_ARG;

	value = espconn_find_connection(espconn, &pnode);
	if (value){
		if (pnode->pespconn->state == ESPCONN_CLOSE)
			return ESPCONN_INPROGRESS;

		espconn_ssl_disconnect(pnode);
		return ESPCONN_OK;
	}
	else
		return ESPCONN_ARG;
}

/******************************************************************************
 * FunctionName : espconn_encry_sent
 * Description  : sent data for client or server
 * Parameters   : espconn -- espconn to set for client or server
 * 				  psent -- data to send
 *                length -- length of data to send
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_secure_sent(struct espconn *espconn, uint8 *psent, uint16 length)
{
	espconn_msg *pnode = NULL;
	bool value = false;
	if (espconn == NULL)
		return ESPCONN_ARG;

	espconn ->state = ESPCONN_WRITE;
	value = espconn_find_connection(espconn, &pnode);
	if (value){
		pmbedtls_msg pssl = NULL;
		pssl = pnode->pssl;
		if (pssl->SentFnFlag){
			pssl->SentFnFlag = false;
			espconn_ssl_sent(pnode, psent, length);
			return ESPCONN_OK;
		}else
			return ESPCONN_INPROGRESS;
	}
	else
		return ESPCONN_ARG;
}

/******************************************************************************
 * FunctionName : espconn_secure_send
 * Description  : send data for client or server
 * Parameters   : espconn -- espconn to set for client or server
 *                psent -- data to send
 *                length -- length of data to send
 * Returns      : none
*******************************************************************************/

sint8 espconn_secure_send(struct espconn *espconn, uint8 *psent, uint16 length) __attribute__((alias("espconn_secure_sent")));

/******************************************************************************
 * FunctionName : espconn_secure_ca_enable
 * Description  : enable the certificate authenticate and set the flash sector
 * 				  as client or server
 * Parameters   : level -- set for client or server
 *				  1: client,2:server,3:client and server
 *				  flash_sector -- flash sector for save certificate
 * Returns      : result true or false
*******************************************************************************/
bool ICACHE_FLASH_ATTR espconn_secure_ca_enable(uint8 level, uint32 flash_sector )
{
	if (flash_sector <= 0)
		return false;

	if (level == ESPCONN_CLIENT){
		ssl_client_options.cert_ca_sector.sector = flash_sector;
		ssl_client_options.cert_ca_sector.flag = true;
		return true;
	}

	return false;
}

/******************************************************************************
 * FunctionName : espconn_secure_ca_disable
 * Description  : disable the certificate authenticate  as client or server
 * Parameters   : level -- set for client or server
 *				  1: client,2:server,3:client and server
 * Returns      : result true or false
*******************************************************************************/
bool ICACHE_FLASH_ATTR espconn_secure_ca_disable(uint8 level)
{
	if (level == ESPCONN_CLIENT) {
		ssl_client_options.cert_ca_sector.flag = false;
		return true;
	}

	return false;
}

/******************************************************************************
 * FunctionName : espconn_secure_cert_req_enable
 * Description  : enable the client certificate authenticate and set the flash sector
 * 				  as client or server
 * Parameters   : level -- set for client or server
 *				  1: client,2:server,3:client and server
 *				  flash_sector -- flash sector for save certificate
 * Returns      : result true or false
*******************************************************************************/
bool ICACHE_FLASH_ATTR espconn_secure_cert_req_enable(uint8 level, uint32 flash_sector )
{
	if (flash_sector <= 0)
		return false;

	if (level == ESPCONN_CLIENT){
		ssl_client_options.cert_req_sector.sector = flash_sector;
		ssl_client_options.cert_req_sector.flag = true;
		return true;
	}

	return false;
}

/******************************************************************************
 * FunctionName : espconn_secure_ca_disable
 * Description  : disable the client certificate authenticate  as client or server
 * Parameters   : level -- set for client or server
 *				  1: client,2:server,3:client and server
 * Returns      : result true or false
*******************************************************************************/
bool ICACHE_FLASH_ATTR espconn_secure_cert_req_disable(uint8 level)
{
	if (level == ESPCONN_CLIENT) {
		ssl_client_options.cert_req_sector.flag = false;
		return true;
	}

	return false;
}

#endif
