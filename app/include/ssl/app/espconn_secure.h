#ifndef __ESPCONN_ENCRY_H__
#define __ESPCONN_ENCRY_H__

#include "espconn/espconn.h"

/******************************************************************************
 * FunctionName : espconn_encry_connect
 * Description  : The function given as connection
 * Parameters   : espconn -- the espconn used to connect with the host
 * Returns      : none
*******************************************************************************/

sint8 espconn_secure_connect(struct espconn *espconn);

/******************************************************************************
 * FunctionName : espconn_encry_disconnect
 * Description  : The function given as the disconnection
 * Parameters   : espconn -- the espconn used to disconnect with the host
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_secure_disconnect(struct espconn *espconn);

/******************************************************************************
 * FunctionName : espconn_encry_sent
 * Description  : sent data for client or server
 * Parameters   : espconn -- espconn to set for client or server
 * 				  psent -- data to send
 *                length -- length of data to send
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_secure_sent(struct espconn *espconn, uint8 *psent, uint16 length);

/******************************************************************************
 * FunctionName : espconn_secure_accept
 * Description  : The function given as the listen
 * Parameters   : espconn -- the espconn used to listen the connection
 * Returns      : none
*******************************************************************************/

extern sint8 espconn_secure_accept(struct espconn *espconn);

#endif


