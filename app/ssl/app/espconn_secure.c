/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: espconn_encry.c
 *
 * Description: data encrypt interface
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

#include "ssl/app/espconn_ssl.h"

/******************************************************************************
 * FunctionName : espconn_encry_connect
 * Description  : The function given as the connect
 * Parameters   : espconn -- the espconn used to listen the connection
 * Returns      : none
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
espconn_secure_connect(struct espconn *espconn)
{	
	if (espconn == NULL)
		return ESPCONN_ARG;
	
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
		espconn_ssl_sent(pnode, psent, length);
		return ESPCONN_OK;
	}
	else
		return ESPCONN_ARG;
}

sint8 ICACHE_FLASH_ATTR
espconn_secure_accept(struct espconn *espconn)
{
	if (espconn == NULL)
		return ESPCONN_ARG;

	return espconn_ssl_server(espconn);
}


