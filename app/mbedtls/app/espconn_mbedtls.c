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

#if !defined(ESPCONN_MBEDTLS)

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include "mbedtls/ssl_internal.h"

#include "mem.h"

#ifdef MEMLEAK_DEBUG
static const char mem_debug_file[] ICACHE_RODATA_ATTR = __FILE__;
#endif

#include "sys/socket.h"
#include "sys/espconn_mbedtls.h"

static os_event_t lwIPThreadQueue[lwIPThreadQueueLen];
static bool lwIPThreadFlag = false;
extern espconn_msg *plink_active;
static espconn_msg *plink_server = NULL;
static pmbedtls_parame def_certificate = NULL;
static pmbedtls_parame def_private_key = NULL;

#if defined(ESP8266_PLATFORM)
#define MBEDTLS_SSL_OUTBUFFER_LEN  ( MBEDTLS_SSL_PLAIN_ADD               \
                        + MBEDTLS_SSL_COMPRESSION_ADD               \
                        + 29 /* counter + header + IV */    \
                        + MBEDTLS_SSL_MAC_ADD                       \
                        + MBEDTLS_SSL_PADDING_ADD                   \
                        )
#endif

/* Implementation that should never be optimized out by the compiler */
static void mbedtls_zeroize( void *v, size_t n ) {
    volatile unsigned char *p = v; while( n-- ) *p++ = 0;
}

static pmbedtls_parame mbedtls_parame_new(size_t capacity)
{
	pmbedtls_parame rb = (pmbedtls_parame)os_zalloc(sizeof(mbedtls_parame));
	if (rb && capacity != 0){
		rb->parame_datalen = capacity;
		rb->parame_data = (uint8*)os_zalloc(rb->parame_datalen + 1);
		if (rb->parame_data){

		} else{
			os_free(rb);
			rb = NULL;
		}
	}
	return rb;
}

static void mbedtls_parame_free(pmbedtls_parame *fp)
{
	lwIP_ASSERT(fp);
	lwIP_ASSERT(*fp);

	os_free((*fp)->parame_data);
	os_free(*fp);
	*fp = NULL;
}

bool mbedtls_load_default_obj(uint32 flash_sector, int obj_type, const unsigned char *load_buf, uint16 length)
{
	pmbedtls_parame mbedtls_write = NULL;
	uint32 mbedtls_head = 0;
	bool mbedtls_load_flag = false;

	if (flash_sector != 0){
		spi_flash_read(flash_sector * FLASH_SECTOR_SIZE, (uint32*)&mbedtls_head, 4);
		if (mbedtls_head != ESPCONN_INVALID_TYPE){
			mbedtls_write = mbedtls_parame_new(0);
			mbedtls_write->parame_datalen = length;
		}
	} else{
		const char* const begin = "-----BEGIN";
		int format_type = ESPCONN_FORMAT_INIT;
		/*
	     * Determine data content. data contains either one DER certificate or
	     * one or more PEM certificates.
	     */
		if ((char*)os_strstr(load_buf, begin) != NULL){
			format_type = ESPCONN_FORMAT_PEM;
		}else{
			format_type = ESPCONN_FORMAT_DER;
		}
		
		if (format_type == ESPCONN_FORMAT_PEM){
			length += 1;
		} 
		
		mbedtls_write = mbedtls_parame_new(length);
		if (mbedtls_write){
			os_memcpy(mbedtls_write->parame_data, load_buf, length);
			if (format_type == ESPCONN_FORMAT_PEM)
				mbedtls_write->parame_data[length - 1] = '\0';		
		}
	}

	if (mbedtls_write){
		mbedtls_load_flag = true;
		mbedtls_write->parame_type = obj_type;
		mbedtls_write->parame_sec = flash_sector;
		if (obj_type == ESPCONN_PK){		
			def_private_key = mbedtls_write;											
		} else{
			def_certificate = mbedtls_write;						
		}
	}
	return mbedtls_load_flag;
}

static unsigned char* mbedtls_get_default_obj(uint32 *sec, uint32 type, uint32 *len)
{
	const char* const begin = "-----BEGIN";
	unsigned char *parame_data = NULL;
	pmbedtls_parame mbedtls_obj = NULL;

	if (type == ESPCONN_PK){		
		mbedtls_obj = def_private_key;											
	} else{
		mbedtls_obj = def_certificate;						
	}
	
	if (mbedtls_obj->parame_sec != 0){
		#define DATA_OFFSET	4
		uint32  data_len = mbedtls_obj->parame_datalen;
		parame_data = (unsigned char *)os_zalloc(data_len + DATA_OFFSET);
		if (parame_data){
			spi_flash_read(mbedtls_obj->parame_sec * FLASH_SECTOR_SIZE, (uint32*)parame_data, data_len);
			/*
		     * Determine buffer content. Buffer contains either one DER certificate or
		     * one or more PEM certificates.
		     */
			if ((char*)os_strstr(parame_data, begin) != NULL){
				data_len ++;
				parame_data[data_len - 1] = '\0';
			}					
		}
		*len = data_len;
	} else{
		parame_data = mbedtls_obj->parame_data;
		*len = mbedtls_obj->parame_datalen;
	}
	
	*sec = mbedtls_obj->parame_sec;	
	return parame_data;
}

static int mbedtls_setsockopt(int sock_id, int level, int optname, int optval)
{
	return setsockopt(sock_id, level, optname, (void*)&optval, sizeof(optval));
}

static int mbedtls_keep_alive(int sock_id, int onoff, int idle, int intvl, int cnt)
{
	int ret = ERR_OK;
	if (onoff == 0)
		return mbedtls_setsockopt(sock_id, SOL_SOCKET, SO_KEEPALIVE, onoff);;

	ret = mbedtls_setsockopt(sock_id, SOL_SOCKET, SO_KEEPALIVE, onoff);
	lwIP_REQUIRE_NOERROR(ret, exit);
	ret = mbedtls_setsockopt(sock_id, IPPROTO_TCP, TCP_KEEPALIVE, onoff);
	lwIP_REQUIRE_NOERROR(ret, exit);
	ret = mbedtls_setsockopt(sock_id, IPPROTO_TCP, TCP_KEEPIDLE, idle);
	lwIP_REQUIRE_NOERROR(ret, exit);
	ret = mbedtls_setsockopt(sock_id, IPPROTO_TCP, TCP_KEEPINTVL, intvl);
	lwIP_REQUIRE_NOERROR(ret, exit);
	ret = mbedtls_setsockopt(sock_id, IPPROTO_TCP, TCP_KEEPCNT, cnt);
	lwIP_REQUIRE_NOERROR(ret, exit);

exit:
	return ret;
}

#if defined(ESP8266_PLATFORM)
static pmbedtls_finished mbedtls_finished_new(int len)
{
    pmbedtls_finished finished = (pmbedtls_finished)os_zalloc(sizeof(mbedtls_finished));
    if (finished)
    {
        finished->finished_len = len;
        finished->finished_buf = (uint8*)os_zalloc(finished->finished_len + 1);
        if (finished->finished_buf)
        {

        }
        else
        {
            os_free(finished);
            finished = NULL;
        }
    }
    return finished;
}

static void mbedtls_finished_free(pmbedtls_finished *pfinished)
{
    lwIP_ASSERT(pfinished);
    lwIP_ASSERT(*pfinished);
    os_free((*pfinished)->finished_buf);
    os_free(*pfinished);
    *pfinished = NULL;
}
#endif
static pmbedtls_espconn mbedtls_espconn_new(void)
{
	pmbedtls_espconn mbedtls_conn = NULL;
	mbedtls_conn = (pmbedtls_espconn)os_zalloc(sizeof(mbedtls_espconn));
	if (mbedtls_conn){
		mbedtls_conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
		if (mbedtls_conn->proto.tcp == NULL){
			os_free(mbedtls_conn);
			mbedtls_conn = NULL;
		}
	}

	return mbedtls_conn;
}

static void mbedtls_espconn_free(pmbedtls_espconn *mbedtlsconn)
{
	lwIP_ASSERT(mbedtlsconn);
	lwIP_ASSERT(*mbedtlsconn);
	
	os_free((*mbedtlsconn)->proto.tcp);
	(*mbedtlsconn)->proto.tcp = NULL;
	
	os_free((*mbedtlsconn));
	*mbedtlsconn = NULL;
}

static pmbedtls_session mbedtls_session_new(void)
{
	pmbedtls_session session = (pmbedtls_session)os_zalloc(sizeof(mbedtls_session));
	if (session){
		mbedtls_x509_crt_init(&session->cacert);
		mbedtls_x509_crt_init(&session->clicert);
		mbedtls_pk_init(&session->pkey);
//		mbedtls_entropy_init(&session->entropy);
	}
	return session;
}

static void mbedtls_session_free(pmbedtls_session *session)
{
	lwIP_ASSERT(session);
	lwIP_ASSERT(*session);

	mbedtls_x509_crt_free(&(*session)->cacert);
	mbedtls_x509_crt_free(&(*session)->clicert);
	mbedtls_pk_free(&(*session)->pkey);
//	mbedtls_entropy_free(&(*session)->entropy);
	os_free(*session);
	*session = NULL;
}

static pmbedtls_msg mbedtls_msg_new(void)
{
	pmbedtls_msg msg = (pmbedtls_msg)os_zalloc( sizeof(mbedtls_msg));
	if (msg) {
		os_bzero(msg, sizeof(mbedtls_msg));
		msg->psession = mbedtls_session_new();
		if (msg->psession){
			mbedtls_net_init(&msg->listen_fd);
			mbedtls_net_init(&msg->fd);
			mbedtls_ssl_init(&msg->ssl);
			mbedtls_ssl_config_init(&msg->conf);		
			mbedtls_ctr_drbg_init(&msg->ctr_drbg);
			mbedtls_entropy_init(&msg->entropy);
		} else{
			os_free(msg);
			msg = NULL;
		}
	}
	return msg;
}

static void mbedtls_msg_server_step(pmbedtls_msg msg)
{
	lwIP_ASSERT(msg);

	/*to prevent memory leaks, ensure that each allocated is deleted at every handshake*/
	if (msg->psession){
		mbedtls_session_free(&msg->psession);
	}
#if defined(ESP8266_PLATFORM)
    if (msg->quiet && msg->ssl.out_buf)
    {
        mbedtls_zeroize(msg->ssl.out_buf, MBEDTLS_SSL_OUTBUFFER_LEN);
        os_free(msg->ssl.out_buf);
        msg->ssl.out_buf = NULL;
    }
#endif
	mbedtls_entropy_free(&msg->entropy);
	mbedtls_ssl_free(&msg->ssl);
	mbedtls_ssl_config_free(&msg->conf);
	mbedtls_ctr_drbg_free(&msg->ctr_drbg);

	/*New connection ensure that each initial for next handshake */
	os_bzero(msg, sizeof(mbedtls_msg));
	msg->psession = mbedtls_session_new();
	if (msg->psession){
		mbedtls_net_init(&msg->fd);
		mbedtls_ssl_init(&msg->ssl);
		mbedtls_ssl_config_init(&msg->conf);
		mbedtls_ctr_drbg_init(&msg->ctr_drbg);
		mbedtls_entropy_init(&msg->entropy);
	}	
}

static void mbedtls_msg_free(pmbedtls_msg *msg)
{
	lwIP_ASSERT(msg);
	lwIP_ASSERT(*msg);

	/*to prevent memory leaks, ensure that each allocated is deleted at every handshake*/
	if ((*msg)->psession){
		mbedtls_session_free(&((*msg)->psession));
	}
#if defined(ESP8266_PLATFORM)
    if ((*msg)->quiet && (*msg)->ssl.out_buf)
    {
        mbedtls_zeroize((*msg)->ssl.out_buf, MBEDTLS_SSL_OUTBUFFER_LEN);
        os_free((*msg)->ssl.out_buf);
        (*msg)->ssl.out_buf = NULL;
    }
    if((*msg)->pfinished != NULL)
        mbedtls_finished_free(&(*msg)->pfinished);
#endif
	mbedtls_entropy_free(&(*msg)->entropy);
	mbedtls_ssl_free(&(*msg)->ssl);
	mbedtls_ssl_config_free(&(*msg)->conf);
	mbedtls_ctr_drbg_free(&(*msg)->ctr_drbg);

	os_free(*msg);
	*msg = NULL;
}

static espconn_msg* mbedtls_msg_find(int sock)
{
	espconn_msg *plist = NULL;
	pmbedtls_msg msg = NULL;

	for (plist = plink_active; plist != NULL; plist = plist->pnext) {
		if(plist->pssl != NULL){
			msg = plist->pssl;
			if (msg->fd.fd == sock)
				return plist;
		}
	}

	for (plist = plink_server; plist != NULL; plist = plist->pnext){
		if(plist->pssl != NULL){
			msg = plist->pssl;
			if (msg->listen_fd.fd == sock)
				return plist;
		}
	}
	return NULL;
}

void mbedtls_handshake_heap(mbedtls_ssl_context *ssl)
{
	os_printf("mbedtls_handshake_heap %d %d\n", ssl->state, system_get_free_heap_size());
}

static bool mbedtls_handshake_result(const pmbedtls_msg Threadmsg)
{
	if (Threadmsg == NULL)
		return false;

	if (Threadmsg->ssl.state == MBEDTLS_SSL_HANDSHAKE_OVER) {
		int ret = 0;
		if (Threadmsg->listen_fd.fd == -1)
			ret = ssl_option.client.cert_ca_sector.flag;
		else
			ret = ssl_option.server.cert_ca_sector.flag;

		if (ret == 1){
			ret = mbedtls_ssl_get_verify_result(&Threadmsg->ssl);
			if (ret != 0) {
				char vrfy_buf[512];
				os_memset(vrfy_buf, 0, sizeof(vrfy_buf)-1);
				mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "!", ret);
				os_printf("%s\n", vrfy_buf);
				Threadmsg->verify_result = ret;
				return false;
			} else
				return true;
		} else
			return true;
	}else
		return false;
}

static void mbedtls_fail_info(espconn_msg *pinfo, int ret)
{
	pmbedtls_msg TLSmsg = NULL;
	lwIP_REQUIRE_ACTION(pinfo,exit,);
	TLSmsg = pinfo->pssl;
	lwIP_REQUIRE_ACTION(TLSmsg,exit,);

	/* Don't complain to console if we've been told the other end is hanging
	 * up.  That's entirely normal and not worthy of the confusion it sows!
	 */
	if (ret != MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
		if (TLSmsg->quiet){
			if (pinfo->preverse != NULL) {
				os_printf("server's data invalid protocol\n");
			} else {
				os_printf("client's data invalid protocol\n");
			}
			mbedtls_ssl_close_notify(&TLSmsg->ssl);
		} else{
			if (pinfo->preverse != NULL) {
				os_printf("server handshake failed!\n");
			} else {
				os_printf("client handshake failed!\n");
			}
		}
	}

	os_printf("Reason:[-0x%2x]\n",-ret);
	/*Error code convert*/
	ret = -ret;
	if ((ret & 0xFF) != 0){
		ret = ((ret >> 8) + ret);
	} else{
		ret >>= 8;
	}
	pinfo->hs_status = -ret;
	pinfo->pespconn->state = ESPCONN_CLOSE;

	mbedtls_net_free(&TLSmsg->fd);
		
exit:
	return;
}

#if defined(ESP8266_PLATFORM)
int mbedtls_write_finished(mbedtls_ssl_context *ssl)
{
    lwIP_ASSERT(ssl);
    lwIP_ASSERT(ssl->p_bio);
    int ret = ERR_OK;
    int fd = ((mbedtls_net_context *) ssl->p_bio)->fd;
    espconn_msg *Threadmsg = mbedtls_msg_find(fd);
    lwIP_REQUIRE_ACTION(Threadmsg, exit, ret = ERR_MEM);
    pmbedtls_msg TLSmsg = Threadmsg->pssl;
    lwIP_REQUIRE_ACTION(TLSmsg, exit, ret = ERR_MEM);
    TLSmsg->pfinished = mbedtls_finished_new(ssl->out_msglen + 29);
    lwIP_REQUIRE_ACTION(TLSmsg->pfinished, exit, ret = ERR_MEM);
    os_memcpy(TLSmsg->pfinished->finished_buf, ssl->out_ctr, TLSmsg->pfinished->finished_len);
exit:
    return ret;
}

static int mbedtls_hanshake_finished(mbedtls_msg *msg)
{
    lwIP_ASSERT(msg);
    int ret = ERR_OK;	
    const size_t len = MBEDTLS_SSL_OUTBUFFER_LEN;

	mbedtls_ssl_context *ssl = &msg->ssl;
    lwIP_REQUIRE_ACTION(ssl, exit, ret = ERR_MEM);

	pmbedtls_finished finished = msg->pfinished;
    lwIP_REQUIRE_ACTION(finished, exit, ret = ERR_MEM);

	ssl->out_buf = (unsigned char*)os_zalloc(len);
	lwIP_REQUIRE_ACTION(ssl->out_buf, exit, ret = MBEDTLS_ERR_SSL_ALLOC_FAILED);
    
    ssl->out_ctr = ssl->out_buf;
    ssl->out_hdr = ssl->out_buf +  8;
    ssl->out_len = ssl->out_buf + 11;
    ssl->out_iv  = ssl->out_buf + 13;
    ssl->out_msg = ssl->out_buf + 29;
    os_memcpy(ssl->out_ctr, finished->finished_buf, finished->finished_len);
    mbedtls_finished_free(&msg->pfinished);

exit:
    return ret;
}
#endif
static void mbedtls_handshake_succ(mbedtls_ssl_context *ssl)
{
	lwIP_ASSERT(ssl);
	if( ssl->handshake )
    {
        mbedtls_ssl_handshake_free( ssl->handshake );
        mbedtls_ssl_transform_free( ssl->transform_negotiate );
        mbedtls_ssl_session_free( ssl->session_negotiate );

        os_free( ssl->handshake );
        os_free( ssl->transform_negotiate );
        os_free( ssl->session_negotiate );
		ssl->handshake = NULL;
		ssl->transform_negotiate = NULL;
		ssl->session_negotiate = NULL;
    }

    if( ssl->session )
    {
        mbedtls_ssl_session_free( ssl->session );
        os_free( ssl->session );
		ssl->session = NULL;
    }

#if defined(MBEDTLS_X509_CRT_PARSE_C)
    if( ssl->hostname != NULL )
    {
        mbedtls_zeroize( ssl->hostname, os_strlen( ssl->hostname ) );
        os_free( ssl->hostname );
		ssl->hostname = NULL;
    }
#endif
}

/******************************************************************************
 * FunctionName : espconn_ssl_reconnect
 * Description  : reconnect with host
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
static void espconn_close_internal(void *arg, netconn_event event_type)
{
	espconn_msg *pssl_recon = arg;
    struct espconn *espconn = NULL;
    sint8 ssl_reerr = 0;
    sint16 hs_status = 0;
    lwIP_ASSERT(pssl_recon);

	espconn = pssl_recon->preverse;
	ssl_reerr = pssl_recon->pcommon.err;
	hs_status = pssl_recon->hs_status;
	if (espconn != NULL) {
		//clear pcommon parameters.
		pssl_recon->pcommon.write_flag = false;
		pssl_recon->pcommon.ptrbuf = NULL;
		pssl_recon->pcommon.cntr = 0;
		pssl_recon->pcommon.err  = 0;
		espconn = pssl_recon->preverse;
	} else {
		espconn = pssl_recon->pespconn;
		os_free(pssl_recon);
		pssl_recon = NULL;
	}
	
	espconn_kill_oldest_pcb();	
	switch (event_type){
		case NETCONN_EVENT_ERROR:
			if (hs_status == ESPCONN_OK)
				ESPCONN_EVENT_ERROR(espconn, ssl_reerr);
			else
				ESPCONN_EVENT_ERROR(espconn, hs_status);
			break;
		case NETCONN_EVENT_CLOSE:
			if (hs_status == ESPCONN_OK)
				ESPCONN_EVENT_CLOSED(espconn);
			else
				ESPCONN_EVENT_ERROR(espconn, hs_status);
			break;
		default:
			break;
	}
}

/******************************************************************************
 * FunctionName : espconn_ssl_read_param_from_flash
 * Description  : load parameter from flash, toggle use two sector by flag value.
 * Parameters   : param--the parame point which write the flash
 * Returns      : none
*******************************************************************************/
static bool espconn_ssl_read_param_from_flash(void *param, uint16 len, int32 offset, mbedtls_auth_info *auth_info)
{
	if (param == NULL || (len + offset) > ESPCONN_SECURE_MAX_SIZE) {
		return false;
	}

	uint32 FILE_PARAM_START_SEC = 0x3B;
	switch (auth_info->auth_level) {
		case ESPCONN_CLIENT:
			switch (auth_info->auth_type) {
				case ESPCONN_CERT_AUTH:
					FILE_PARAM_START_SEC = ssl_option.client.cert_ca_sector.sector;
					break;
				case ESPCONN_CERT_OWN:
				case ESPCONN_PK:
					FILE_PARAM_START_SEC = ssl_option.client.cert_req_sector.sector;
					break;
				default:
					return false;
			}
			break;
		case ESPCONN_SERVER:
			switch (auth_info->auth_type) {
				case ESPCONN_CERT_AUTH:
					FILE_PARAM_START_SEC = ssl_option.server.cert_ca_sector.sector;
					break;
				case ESPCONN_CERT_OWN:
				case ESPCONN_PK:
					FILE_PARAM_START_SEC = ssl_option.server.cert_req_sector.sector;
					break;
				default:
					return false;
			}
			break;
		default:
			return false;
			break;
	}

	spi_flash_read(FILE_PARAM_START_SEC * 4096 + offset, param, len);

	return true;
}

static bool mbedtls_msg_info_load(mbedtls_msg *msg, mbedtls_auth_info *auth_info)
{
	const char* const begin = "-----BEGIN";
	const char* const type_name  = "private_key";
	#define FILE_OFFSET	4
	int ret = 0;
	int32 offerset = 0;
	uint8* load_buf = NULL;
	size_t load_len = 0;
	file_param *pfile_param = NULL;
	pfile_param = (file_param *)os_zalloc( sizeof(file_param));
	if (pfile_param==NULL)
		return false;

again:
	espconn_ssl_read_param_from_flash(&pfile_param->file_head, sizeof(file_head), offerset, auth_info);
	pfile_param->file_offerset = offerset;
	os_printf("%s %d, type[%s],length[%d]\n", __FILE__, __LINE__, pfile_param->file_head.file_name, pfile_param->file_head.file_length);
	if (pfile_param->file_head.file_length == 0xFFFF){
		os_free(pfile_param);
		return false;
	} else{
		/*Optional is load the private key*/
		if (auth_info->auth_type == ESPCONN_PK && os_memcmp(pfile_param->file_head.file_name, type_name, os_strlen(type_name)) != 0){
			offerset += sizeof(file_head) + pfile_param->file_head.file_length;
			goto again;
		}
		/*Optional is load the cert*/
		if (auth_info->auth_type == ESPCONN_CERT_OWN && os_memcmp(pfile_param->file_head.file_name, "certificate", os_strlen("certificate")) != 0){
			offerset += sizeof(file_head) + pfile_param->file_head.file_length;
			goto again;
		}
		load_buf = (uint8_t *) os_zalloc( pfile_param->file_head.file_length + FILE_OFFSET);
		if (load_buf == NULL){
			os_free(pfile_param);
			return false;
		}
		offerset = sizeof(file_head) + pfile_param->file_offerset;
		espconn_ssl_read_param_from_flash(load_buf,	pfile_param->file_head.file_length, offerset, auth_info);
	}

	load_len = pfile_param->file_head.file_length;
	 /*
     * Determine buffer content. Buffer contains either one DER certificate or
     * one or more PEM certificates.
     */
	if ((char*)os_strstr(load_buf, begin) != NULL){
		load_len += 1;
		load_buf[load_len - 1] = '\0';
	}
	switch (auth_info->auth_type){
	case ESPCONN_CERT_AUTH:
		/*Optional is not optimal for security*/
		ret = mbedtls_x509_crt_parse(&msg->psession->cacert, (const uint8*) load_buf,load_len);
		lwIP_REQUIRE_NOERROR(ret, exit);
		mbedtls_ssl_conf_authmode(&msg->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
		mbedtls_ssl_conf_ca_chain(&msg->conf, &msg->psession->cacert, NULL);
		break;
	case ESPCONN_CERT_OWN:
		ret = mbedtls_x509_crt_parse(&msg->psession->clicert, (const uint8*) load_buf,load_len);
		break;
	case ESPCONN_PK:
		ret = mbedtls_pk_parse_key(&msg->psession->pkey, (const uint8*) load_buf,load_len, NULL, 0);
		lwIP_REQUIRE_NOERROR(ret, exit);
		ret = mbedtls_ssl_conf_own_cert(&msg->conf, &msg->psession->clicert, &msg->psession->pkey);
		break;
	}
exit:	
	os_free(load_buf);
	os_free(pfile_param);
	if (ret < 0){
		return false;
	}else{
		return true;
	}
}

static void
mbedtls_dbg(void *p, int level, const char *file, int line, const char *str)
{
	os_printf("TLS<%d> (heap=%d): %s:%d %s", level, system_get_free_heap_size(), file, line, str);
}

static bool mbedtls_msg_config(mbedtls_msg *msg)
{
	const char *pers = NULL;
	uint8 auth_type = 0;
	bool load_flag = false;
	int ret = ESPCONN_OK;
	mbedtls_auth_info auth_info;

	/*end_point mode*/
	if (msg->listen_fd.fd == -1){
		pers = "client";
		auth_type = MBEDTLS_SSL_IS_CLIENT;
	} else {
		pers = "server";
		auth_type = MBEDTLS_SSL_IS_SERVER;
	}

	/*Initialize the RNG and the session data*/
	ret = mbedtls_ctr_drbg_seed(&msg->ctr_drbg, mbedtls_entropy_func, &msg->entropy, (const unsigned char*) pers, os_strlen(pers));
	lwIP_REQUIRE_NOERROR(ret, exit);

	if (auth_type == MBEDTLS_SSL_IS_SERVER){
		uint32 flash_sector = 0;		
		/*Load the certificate*/
		unsigned int def_certificate_len = 0;unsigned char *def_certificate = NULL;
		def_certificate = (unsigned char *)mbedtls_get_default_obj(&flash_sector,ESPCONN_CERT_OWN, &def_certificate_len);
		lwIP_REQUIRE_ACTION(def_certificate, exit, ret = MBEDTLS_ERR_SSL_ALLOC_FAILED);
		ret = mbedtls_x509_crt_parse(&msg->psession->clicert, (const unsigned char *)def_certificate, def_certificate_len);
		if (flash_sector != 0)
			os_free(def_certificate);
		lwIP_REQUIRE_NOERROR(ret, exit);

		/*Load the private RSA key*/
		unsigned int def_private_key_len = 0;unsigned char *def_private_key = NULL;
		def_private_key = (unsigned char *)mbedtls_get_default_obj(&flash_sector,ESPCONN_PK, &def_private_key_len);
		lwIP_REQUIRE_ACTION(def_private_key, exit, ret = MBEDTLS_ERR_SSL_ALLOC_FAILED);
		ret = mbedtls_pk_parse_key(&msg->psession->pkey, (const unsigned char *)def_private_key, def_private_key_len, NULL, 0);
		if (flash_sector != 0)
			os_free(def_private_key);
		lwIP_REQUIRE_NOERROR(ret, exit);
		ret = mbedtls_ssl_conf_own_cert(&msg->conf, &msg->psession->clicert, &msg->psession->pkey);
		lwIP_REQUIRE_NOERROR(ret, exit);

		/*Load the trusted CA*/
		if (ssl_option.server.cert_ca_sector.flag) {
			auth_info.auth_level = ESPCONN_SERVER;
			auth_info.auth_type = ESPCONN_CERT_AUTH;
			load_flag = mbedtls_msg_info_load(msg, &auth_info);
			lwIP_REQUIRE_ACTION(load_flag, exit, ret = ESPCONN_MEM);
		}
	} else{
		/*Load the certificate and private RSA key*/
		if (ssl_option.client.cert_req_sector.flag) {
			auth_info.auth_level = ESPCONN_CLIENT;
			auth_info.auth_type = ESPCONN_CERT_OWN;
			load_flag = mbedtls_msg_info_load(msg, &auth_info);
			lwIP_REQUIRE_ACTION(load_flag, exit, ret = ESPCONN_MEM);
			auth_info.auth_type = ESPCONN_PK;
			load_flag = mbedtls_msg_info_load(msg, &auth_info);
			lwIP_REQUIRE_ACTION(load_flag, exit, ret = ESPCONN_MEM);
		}

		/*Load the trusted CA*/
		if(ssl_option.client.cert_ca_sector.flag){
			auth_info.auth_level = ESPCONN_CLIENT;
			auth_info.auth_type = ESPCONN_CERT_AUTH;
			load_flag = mbedtls_msg_info_load(msg, &auth_info);
			lwIP_REQUIRE_ACTION(load_flag, exit, ret = ESPCONN_MEM);
		}
	}

	/*Setup the stuff*/
	ret = mbedtls_ssl_config_defaults(&msg->conf, auth_type, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
	lwIP_REQUIRE_NOERROR(ret, exit);

	/*OPTIONAL is not optimal for security, but makes interop easier in this session*/
	if (auth_type == MBEDTLS_SSL_IS_CLIENT && ssl_option.client.cert_ca_sector.flag == false){
		mbedtls_ssl_conf_authmode(&msg->conf, MBEDTLS_SSL_VERIFY_NONE);
	}
	mbedtls_ssl_conf_rng(&msg->conf, mbedtls_ctr_drbg_random, &msg->ctr_drbg);
	mbedtls_ssl_conf_dbg(&msg->conf, mbedtls_dbg, NULL);
	
	ret = mbedtls_ssl_setup(&msg->ssl, &msg->conf);
	lwIP_REQUIRE_NOERROR(ret, exit);

	mbedtls_ssl_set_bio(&msg->ssl, &msg->fd, mbedtls_net_send, mbedtls_net_recv, NULL);

exit:
	if (ret != 0){
		return false;
	} else{
		return true;
	}
}

int __attribute__((weak)) mbedtls_parse_internal(int socket, sint8 error)
{
	int ret = ERR_OK;
	bool config_flag = false;
	espconn_msg *Threadmsg = NULL;
	pmbedtls_msg TLSmsg = NULL;
	Threadmsg = mbedtls_msg_find(socket);
	lwIP_REQUIRE_ACTION(Threadmsg, exit, ret = ERR_MEM);
	TLSmsg = Threadmsg->pssl;
	lwIP_REQUIRE_ACTION(TLSmsg, exit, ret = ERR_MEM);

	if (error == ERR_OK){
		if (TLSmsg->quiet){
			uint8 *TheadBuff = NULL;
			size_t ThreadLen = MBEDTLS_SSL_PLAIN_ADD;
			TheadBuff = (uint8 *)os_zalloc(ThreadLen + 1);
			lwIP_REQUIRE_ACTION(TheadBuff, exit, ret = ERR_MEM);
			do {
				os_memset(TheadBuff, 0, ThreadLen);
				ret = mbedtls_ssl_read(&TLSmsg->ssl, TheadBuff, ThreadLen);
				if (ret > 0){
					ESPCONN_EVENT_RECV(Threadmsg->pespconn, TheadBuff, ret);
				} else{
					if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == 0){
						ret = ESPCONN_OK;
						break;
					} else if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY){
						ret = ESPCONN_OK;
						mbedtls_ssl_close_notify(&TLSmsg->ssl);
					} else{
						break;
					}
				}				
			} while(1);
			os_free(TheadBuff);
			TheadBuff = NULL;
			lwIP_REQUIRE_NOERROR(ret, exit);
		} else{
			if (TLSmsg->ssl.state == MBEDTLS_SSL_HELLO_REQUEST){
				if (Threadmsg->preverse != NULL){
					struct espconn *accept_conn = NULL;
					struct espconn *espconn = Threadmsg->preverse;
					struct sockaddr_in name;
					socklen_t name_len = sizeof(name);
					remot_info *pinfo = NULL;
					espconn_get_connection_info(espconn, &pinfo , ESPCONN_SSL);
					if (espconn->link_cnt == 0x01)
						return ERR_ISCONN;

					ret = mbedtls_net_accept(&TLSmsg->listen_fd, &TLSmsg->fd, NULL, 0, NULL);
					lwIP_REQUIRE_NOERROR(ret, exit);
					accept_conn = mbedtls_espconn_new();
					lwIP_REQUIRE_ACTION(accept_conn, exit, ret = ERR_MEM);
					Threadmsg->pespconn = accept_conn;
					/*get the remote information*/
					getpeername(TLSmsg->fd.fd, (struct sockaddr*)&name, &name_len);
					Threadmsg->pcommon.remote_port = htons(name.sin_port);
					os_memcpy(Threadmsg->pcommon.remote_ip, &name.sin_addr.s_addr, 4);
					
					espconn->proto.tcp->remote_port = htons(name.sin_port);
					os_memcpy(espconn->proto.tcp->remote_ip, &name.sin_addr.s_addr, 4);
					
					espconn_copy_partial(accept_conn, espconn);					

					/*insert the node to the active connection list*/
					espconn_list_creat(&plink_active, Threadmsg);
					os_printf("server handshake start.\n");
				} else{
					os_printf("client handshake start.\n");
				}
				config_flag = mbedtls_msg_config(TLSmsg);
				if (config_flag){
//					mbedtls_keep_alive(TLSmsg->fd.fd, 1, SSL_KEEP_IDLE, SSL_KEEP_INTVL, SSL_KEEP_CNT);
					system_overclock();
				} else{
					ret = MBEDTLS_ERR_SSL_ALLOC_FAILED;
					lwIP_REQUIRE_NOERROR(ret, exit);
				}
			}
			
			system_soft_wdt_stop();
			uint8 cpu_freq;
			cpu_freq = system_get_cpu_freq();
			system_update_cpu_freq(160);
			while ((ret = mbedtls_ssl_handshake(&TLSmsg->ssl)) != 0) {

				if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
					ret = ESPCONN_OK;
					break;
				} else{
					break;
				}
			}
			system_soft_wdt_restart();
			system_update_cpu_freq(cpu_freq);
			lwIP_REQUIRE_NOERROR(ret, exit);
			/**/
			TLSmsg->quiet = mbedtls_handshake_result(TLSmsg);
			if (TLSmsg->quiet){
				if (Threadmsg->preverse != NULL) {
					os_printf("server handshake ok!\n");
				} else {
					os_printf("client handshake ok!\n");
				}
//				mbedtls_keep_alive(TLSmsg->fd.fd, 0, SSL_KEEP_IDLE, SSL_KEEP_INTVL, SSL_KEEP_CNT);
				mbedtls_session_free(&TLSmsg->psession);
				mbedtls_handshake_succ(&TLSmsg->ssl);
#if defined(ESP8266_PLATFORM)
                mbedtls_hanshake_finished(TLSmsg);
#endif
				system_restoreclock();
				
				TLSmsg->SentFnFlag = true;
				ESPCONN_EVENT_CONNECTED(Threadmsg->pespconn);
			} else{
				lwIP_REQUIRE_NOERROR_ACTION(TLSmsg->verify_result, exit, ret = TLSmsg->verify_result);
			}
		}
	} else if (error < 0){
		Threadmsg->pcommon.err = error;
		Threadmsg->pespconn->state = ESPCONN_CLOSE;
		mbedtls_net_free(&TLSmsg->fd);
		ets_post(lwIPThreadPrio, NETCONN_EVENT_ERROR, (uint32)Threadmsg);
	} else {
		ret = MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
		lwIP_REQUIRE_NOERROR(ret, exit);
	}

exit:
	if (ret != ESPCONN_OK){
		mbedtls_fail_info(Threadmsg, ret);		
		if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY){
			Threadmsg->hs_status = ESPCONN_OK;
		}
		ets_post(lwIPThreadPrio, NETCONN_EVENT_CLOSE,(uint32)Threadmsg);
	}
	return ret;
}

int __attribute__((weak)) mbedtls_parse_thread(int socket, int event, int error)
{
	int ret = ERR_OK;
	espconn_msg *Threadmsg = NULL;
	pmbedtls_msg TLSmsg = NULL;
	Threadmsg = mbedtls_msg_find(socket);
	lwIP_REQUIRE_ACTION(Threadmsg, exit, ret = ERR_MEM);
	TLSmsg = Threadmsg->pssl;
	lwIP_REQUIRE_ACTION(TLSmsg, exit, ret = ERR_MEM);
	if (TLSmsg->quiet){
		int	out_msglen = TLSmsg->ssl.out_msglen + 5;
		if (Threadmsg->pcommon.write_flag)
			TLSmsg->record.record_len += error;
		
		if (TLSmsg->record.record_len == out_msglen){
			TLSmsg->record.record_len = 0;
			Threadmsg->pcommon.write_flag = false;
			if (Threadmsg->pcommon.cntr != 0){
				espconn_ssl_sent(Threadmsg, Threadmsg->pcommon.ptrbuf, Threadmsg->pcommon.cntr);
			} else{
				TLSmsg->SentFnFlag = true;
				ESPCONN_EVENT_SEND(Threadmsg->pespconn);
			}
		} else{

		}
	} else{

	}
exit:
	return ret;
}

/**
  * @brief  Api_Thread.
  * @param  events: contain the Api_Thread processing data
  * @retval None
  */
static void
mbedtls_thread(os_event_t *events)
{
	int ret = ESP_OK;
	bool active_flag = false;
	espconn_msg *Threadmsg = NULL;
	espconn_msg *ListMsg = NULL;
	pmbedtls_msg TLSmsg = NULL;
	Threadmsg = (espconn_msg *)events->par;
	lwIP_REQUIRE_ACTION(Threadmsg,exit,ret = ERR_ARG);
	TLSmsg = Threadmsg->pssl;
	lwIP_REQUIRE_ACTION(TLSmsg,exit,ret = ERR_ARG);
	lwIP_REQUIRE_ACTION(Threadmsg->pespconn,exit,ret = ERR_ARG);

	/*find the active connection*/
	for (ListMsg = plink_active; ListMsg != NULL; ListMsg = ListMsg->pnext){
		if (Threadmsg == ListMsg){
			active_flag = true;
			break;
		}
	}
	
	if (active_flag){
		/*remove the node from the active connection list*/
		espconn_list_delete(&plink_active, Threadmsg);
		if (TLSmsg->listen_fd.fd != -1){
			mbedtls_msg_server_step(TLSmsg);
			espconn_copy_partial(Threadmsg->preverse, Threadmsg->pespconn);	
			mbedtls_espconn_free(&Threadmsg->pespconn);
		} else{
			mbedtls_msg_free(&TLSmsg);
			Threadmsg->pssl = NULL;
		}

		switch (events->sig){
			case NETCONN_EVENT_ERROR:
				espconn_close_internal(Threadmsg, NETCONN_EVENT_ERROR);
				break;
			case NETCONN_EVENT_CLOSE:
				espconn_close_internal(Threadmsg, NETCONN_EVENT_CLOSE);
				break;
			default:
				break;
		}
	}
exit:
	return;
}

static void mbedtls_threadinit(void)
{
	ets_task(mbedtls_thread, lwIPThreadPrio, lwIPThreadQueue, lwIPThreadQueueLen);
	lwIPThreadFlag = true;
}

sint8 espconn_ssl_client(struct espconn *espconn)
{
	int ret = ESPCONN_OK;
	struct ip_addr ipaddr;
	const char *server_name = NULL;
	const char *server_port = NULL;
	espconn_msg *pclient = NULL;
	pmbedtls_msg mbedTLSMsg = NULL;
	if (lwIPThreadFlag == false)
		mbedtls_threadinit();

	lwIP_REQUIRE_ACTION(espconn, exit, ret = ESPCONN_ARG);
	pclient = (espconn_msg *)os_zalloc( sizeof(espconn_msg));
	lwIP_REQUIRE_ACTION(pclient, exit, ret = ESPCONN_MEM);
	mbedTLSMsg = mbedtls_msg_new();
	lwIP_REQUIRE_ACTION(mbedTLSMsg, exit, ret = ESPCONN_MEM);
	IP4_ADDR(&ipaddr, espconn->proto.tcp->remote_ip[0],espconn->proto.tcp->remote_ip[1],
	                  espconn->proto.tcp->remote_ip[2],espconn->proto.tcp->remote_ip[3]);
	server_name = ipaddr_ntoa(&ipaddr);
	server_port = (const char *)sys_itoa(espconn->proto.tcp->remote_port);
	
	/*start the connection*/
	ret = mbedtls_net_connect(&mbedTLSMsg->fd, server_name, server_port, MBEDTLS_NET_PROTO_TCP);
	lwIP_REQUIRE_NOERROR_ACTION(ret, exit, ret = ESPCONN_MEM);
	espconn->state = ESPCONN_WAIT;
	pclient->pespconn = espconn;
	pclient->pssl = mbedTLSMsg;
	pclient->preverse = NULL;
	/*insert the node to the active connection list*/
	espconn_list_creat(&plink_active, pclient);
exit:
	if (ret != ESPCONN_OK){
		if (mbedTLSMsg != NULL)
			mbedtls_msg_free(&mbedTLSMsg);
		if (pclient != NULL)
			os_free(pclient);
	}
	return ret;
}

/******************************************************************************
 * FunctionName : espconn_ssl_server
 * Description  : as
 * Parameters   :
 * Returns      :
*******************************************************************************/
sint8  espconn_ssl_server(struct espconn *espconn)
{
	int ret = ESPCONN_OK;
	struct ip_addr ipaddr;

	const char *server_port = NULL;
	espconn_msg *pserver = NULL;
	pmbedtls_msg mbedTLSMsg = NULL;
	if (lwIPThreadFlag == false)
		mbedtls_threadinit();

	if (plink_server != NULL)
		return ESPCONN_INPROGRESS;
		
	lwIP_REQUIRE_ACTION(espconn, exit, ret = ESPCONN_ARG);
	/*Creates a new server control message*/
	pserver = (espconn_msg *) os_zalloc( sizeof(espconn_msg));
	lwIP_REQUIRE_ACTION(espconn, exit, ret = ESPCONN_MEM);
	mbedTLSMsg = mbedtls_msg_new();
	lwIP_REQUIRE_ACTION(mbedTLSMsg, exit, ret = ESPCONN_MEM);

	server_port = (const char *)sys_itoa(espconn->proto.tcp->local_port);	
	/*start the connection*/
	ret = mbedtls_net_bind(&mbedTLSMsg->listen_fd, NULL, server_port, MBEDTLS_NET_PROTO_TCP);
	lwIP_REQUIRE_NOERROR_ACTION(ret, exit, ret = ESPCONN_MEM);
	espconn->state = ESPCONN_LISTEN;
	pserver->pespconn = NULL;
	pserver->pssl = mbedTLSMsg;
	pserver->preverse = espconn;
	pserver->count_opt = MEMP_NUM_TCP_PCB;
	pserver->pcommon.timeout = 0x0a;
	espconn->state = ESPCONN_LISTEN;
	plink_server = pserver;
exit:
	if (ret != ESPCONN_OK) {
		if (mbedTLSMsg != NULL)
			mbedtls_msg_free(&mbedTLSMsg);
		if (pserver != NULL)
			os_free(pserver);
	}
	return ret;
}

/******************************************************************************
 * FunctionName : espconn_ssl_delete
 * Description  : delete the server: delete a listening PCB and free it
 * Parameters   : pdeletecon -- the espconn used to delete a server
 * Returns      : none
*******************************************************************************/
sint8  espconn_ssl_delete(struct espconn *pdeletecon)
{
	remot_info *pinfo = NULL;
	espconn_msg *pdelete_msg = NULL;
	pmbedtls_msg mbedTLSMsg = NULL;

	if (pdeletecon == NULL)
		return ESPCONN_ARG;

	espconn_get_connection_info(pdeletecon, &pinfo, ESPCONN_SSL);
	/*make sure all the active connection have been disconnect*/
	if (pdeletecon->link_cnt != 0)
		return ESPCONN_INPROGRESS;
	else {
		pdelete_msg = plink_server;
		if (pdelete_msg != NULL && pdelete_msg->preverse == pdeletecon) {
			mbedTLSMsg = pdelete_msg->pssl;
			espconn_kill_pcb(pdeletecon->proto.tcp->local_port);
			mbedtls_net_free(&mbedTLSMsg->listen_fd);
			mbedtls_msg_free(&mbedTLSMsg);
			pdelete_msg->pssl = mbedTLSMsg;
			os_free(pdelete_msg);
			pdelete_msg = NULL;
			plink_server = pdelete_msg;
			mbedtls_parame_free(&def_private_key);		
			mbedtls_parame_free(&def_certificate);
			return ESPCONN_OK;
		} else {
			return ESPCONN_ARG;
		}
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
void espconn_ssl_sent(void *arg, uint8 *psent, uint16 length)
{
	espconn_msg *Threadmsg = arg;
	uint16 out_msglen = length; 
	int ret = ESPCONN_OK;
	lwIP_ASSERT(Threadmsg);
	lwIP_ASSERT(psent);
	lwIP_ASSERT(length);
	pmbedtls_msg mbedTLSMsg = Threadmsg->pssl;
	lwIP_ASSERT(mbedTLSMsg);

	if (length > MBEDTLS_SSL_PLAIN_ADD){
		out_msglen = MBEDTLS_SSL_PLAIN_ADD;
	}

	Threadmsg->pcommon.write_flag = true;
	ret = mbedtls_ssl_write(&mbedTLSMsg->ssl, psent, out_msglen);
	if (ret > 0){
		Threadmsg->pcommon.ptrbuf = psent + ret;
		Threadmsg->pcommon.cntr = length - ret;
	} else{
		if (ret == MBEDTLS_ERR_SSL_WANT_WRITE || ret == 0) {
			
		} else{
			mbedtls_fail_info(Threadmsg, ret);		
			ets_post(lwIPThreadPrio, NETCONN_EVENT_CLOSE,(uint32)Threadmsg);
		}
	}
	
}

/******************************************************************************
 * FunctionName : espconn_ssl_disconnect
 * Description  : A new incoming connection has been disconnected.
 * Parameters   : espconn -- the espconn used to disconnect with host
 * Returns      : none
*******************************************************************************/
void espconn_ssl_disconnect(espconn_msg *Threadmsg)
{
	lwIP_ASSERT(Threadmsg);
	pmbedtls_msg mbedTLSMsg = Threadmsg->pssl;
	lwIP_ASSERT(mbedTLSMsg);
	mbedtls_net_free(&mbedTLSMsg->fd);
	Threadmsg->pespconn->state = ESPCONN_CLOSE;
	ets_post(lwIPThreadPrio, NETCONN_EVENT_CLOSE, (uint32)Threadmsg);
}

/*
 * Checkup routine
 */
int mbedtls_x509_test(int verbose,  char *ca_crt,  size_t ca_crt_len, char *cli_crt, size_t cli_crt_len)
{
#if defined(MBEDTLS_SHA1_C)
    int ret;
    uint32_t flags;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt clicert;

    if( verbose != 0 )
        os_printf( "  X.509 certificate load: " );

    mbedtls_x509_crt_init( &clicert );

    ret = mbedtls_x509_crt_parse( &clicert, (const unsigned char *) cli_crt,
                           cli_crt_len );
    if( ret != 0 )
    {
        if( verbose != 0 )
            os_printf( "failed\n" );

        return( ret );
    }

    mbedtls_x509_crt_init( &cacert );

    ret = mbedtls_x509_crt_parse( &cacert, (const unsigned char *) ca_crt,
                          ca_crt_len );
    if( ret != 0 )
    {
        if( verbose != 0 )
            os_printf( "failed\n" );

        return( ret );
    }

    if( verbose != 0 )
        os_printf( "passed\n  X.509 signature verify: ");

    ret = mbedtls_x509_crt_verify( &clicert, &cacert, NULL, NULL, &flags, NULL, NULL );
    if( ret != 0 )
    {
        if( verbose != 0 )
            os_printf( "failed\n" );

        return( ret );
    }

    if( verbose != 0 )
        os_printf( "passed\n\n");

    mbedtls_x509_crt_free( &cacert  );
    mbedtls_x509_crt_free( &clicert );

    return( 0 );
#else
    ((void) verbose);
    return( 0 );
#endif /* MBEDTLS_CERTS_C && MBEDTLS_SHA1_C */
}

#endif
