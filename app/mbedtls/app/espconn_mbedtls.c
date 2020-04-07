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

#include "lauxlib.h"

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

/* Implementation that should never be optimized out by the compiler */
static void mbedtls_zeroize( void *v, size_t n ) {
	volatile unsigned char *p = v;
	while( n-- ) *p++ = 0;
}

static pmbedtls_parame mbedtls_parame_new(size_t capacity)
{
	pmbedtls_parame rb = (pmbedtls_parame)os_zalloc(sizeof(mbedtls_parame));
	if (rb && capacity != 0) {
		rb->parame_datalen = capacity;
		rb->parame_data = (uint8*)os_zalloc(rb->parame_datalen + 1);
		if (rb->parame_data) {

		} else {
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

static unsigned char* mbedtls_get_default_obj(uint32 *sec, uint32 type, uint32 *len)
{
	const char* const begin = "-----BEGIN";
	unsigned char *parame_data = NULL;
	pmbedtls_parame mbedtls_obj = NULL;

	if (type == ESPCONN_PK) {
		mbedtls_obj = def_private_key;
	} else {
		mbedtls_obj = def_certificate;
	}

	if (mbedtls_obj->parame_sec != 0) {
#define DATA_OFFSET	4
		uint32  data_len = mbedtls_obj->parame_datalen;
		parame_data = (unsigned char *)os_zalloc(data_len + DATA_OFFSET);
		if (parame_data) {
			spi_flash_read(mbedtls_obj->parame_sec * FLASH_SECTOR_SIZE, (uint32*)parame_data, data_len);
			/*
			 * Determine buffer content. Buffer contains either one DER certificate or
			 * one or more PEM certificates.
			 */
			if ((char*)os_strstr(parame_data, begin) != NULL) {
				data_len ++;
				parame_data[data_len - 1] = '\0';
			}
		}
		*len = data_len;
	} else {
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

static pmbedtls_espconn mbedtls_espconn_new(void)
{
	pmbedtls_espconn mbedtls_conn = NULL;
	mbedtls_conn = (pmbedtls_espconn)os_zalloc(sizeof(mbedtls_espconn));
	if (mbedtls_conn) {
		mbedtls_conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
		if (mbedtls_conn->proto.tcp == NULL) {
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
	if (session) {
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
		if (msg->psession) {
			mbedtls_net_init(&msg->fd);
			mbedtls_ssl_init(&msg->ssl);
			mbedtls_ssl_config_init(&msg->conf);
			mbedtls_ctr_drbg_init(&msg->ctr_drbg);
			mbedtls_entropy_init(&msg->entropy);
#if defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH) && defined(SSL_MAX_FRAGMENT_LENGTH_CODE)
			mbedtls_ssl_conf_max_frag_len(&msg->conf, SSL_MAX_FRAGMENT_LENGTH_CODE);
#endif
		} else {
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
	if (msg->psession) {
		mbedtls_session_free(&msg->psession);
	}
	mbedtls_entropy_free(&msg->entropy);
	mbedtls_ssl_free(&msg->ssl);
	mbedtls_ssl_config_free(&msg->conf);
	mbedtls_ctr_drbg_free(&msg->ctr_drbg);

	/*New connection ensure that each initial for next handshake */
	os_bzero(msg, sizeof(mbedtls_msg));
	msg->psession = mbedtls_session_new();
	if (msg->psession) {
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
	if ((*msg)->psession) {
		mbedtls_session_free(&((*msg)->psession));
	}
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
		if(plist->pssl != NULL) {
			msg = plist->pssl;
			if (msg->fd.fd == sock)
				return plist;
		}
	}

	return NULL;
}

static bool mbedtls_handshake_result(const pmbedtls_msg Threadmsg)
{
	if (Threadmsg == NULL)
		return false;

	if (Threadmsg->ssl.state == MBEDTLS_SSL_HANDSHAKE_OVER) {
		if (ssl_client_options.cert_ca_sector.flag) {
			int ret = mbedtls_ssl_get_verify_result(&Threadmsg->ssl);
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
	} else
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
		if (TLSmsg->quiet) {
			os_printf("client's data invalid protocol\n");
			mbedtls_ssl_close_notify(&TLSmsg->ssl);
		} else {
			os_printf("client handshake failed!\n");
		}
	}

	os_printf("Reason:[-0x%2x]\n",-ret);
	/*Error code convert*/
	ret = -ret;
	if ((ret & 0xFF) != 0) {
		ret = ((ret >> 8) + ret);
	} else {
		ret >>= 8;
	}
	pinfo->hs_status = -ret;
	pinfo->pespconn->state = ESPCONN_CLOSE;

	mbedtls_net_free(&TLSmsg->fd);

exit:
	return;
}

static void mbedtls_handshake_succ(mbedtls_ssl_context *ssl)
{
	lwIP_ASSERT(ssl);
	if( ssl->handshake )
	{
		mbedtls_ssl_handshake_free( ssl );
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
	} else {
		espconn = pssl_recon->pespconn;
		os_free(pssl_recon);
		pssl_recon = NULL;
	}

	espconn_kill_oldest_pcb();
	switch (event_type) {
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
static bool espconn_ssl_read_param_from_flash(void *param, uint16 len, int32 offset, mbedtls_auth_type auth_type)
{
	if (param == NULL || (len + offset) > ESPCONN_SECURE_MAX_SIZE) {
		return false;
	}

	uint32 FILE_PARAM_START_SEC = 0x3B;
	switch (auth_type) {
	case ESPCONN_CERT_AUTH:
		FILE_PARAM_START_SEC = ssl_client_options.cert_ca_sector.sector;
		break;
	case ESPCONN_CERT_OWN:
	case ESPCONN_PK:
		FILE_PARAM_START_SEC = ssl_client_options.cert_req_sector.sector;
		break;
	default:
		return false;
	}

	spi_flash_read(FILE_PARAM_START_SEC * 4096 + offset, param, len);

	return true;
}

static bool
espconn_mbedtls_parse(mbedtls_msg *msg, mbedtls_auth_type auth_type, const uint8_t *buf, size_t len)
{
	int ret;

	switch (auth_type) {
	case ESPCONN_CERT_AUTH:
		ret = mbedtls_x509_crt_parse(&msg->psession->cacert, buf, len);
		break;
	case ESPCONN_CERT_OWN:
		ret = mbedtls_x509_crt_parse(&msg->psession->clicert, buf, len);
		break;
	case ESPCONN_PK:
		ret = mbedtls_pk_parse_key(&msg->psession->pkey, buf, len, NULL, 0);
		break;
	default:
		return false;
	}

exit:
	return (ret >= 0);
}

/*
 * Three-way return:
 *   0 for no commitment, -1 to fail the connection, 1 on success
 */
static int
nodemcu_tls_cert_get(mbedtls_msg *msg, mbedtls_auth_type auth_type)
{
	int cbref;
	int cbarg;
	int loop = 0;

	switch(auth_type) {
	case ESPCONN_CERT_AUTH:
		loop = 1;
		cbarg = 1;
		cbref = ssl_client_options.cert_verify_callback;
		break;
	case ESPCONN_PK:
		loop = 0;
		cbarg = 0;
		cbref = ssl_client_options.cert_auth_callback;
		break;
	case ESPCONN_CERT_OWN:
		loop = 1;
		cbarg = 1;
		cbref = ssl_client_options.cert_auth_callback;
		break;
	default:
		return 0;
	}

	if (cbref == LUA_NOREF) {
		return 0;
	}

	lua_State *L = lua_getstate();

	do {
		lua_rawgeti(L, LUA_REGISTRYINDEX, cbref);
		lua_pushinteger(L, cbarg);
		if (lua_pcall(L, 1, 1, 0) != 0) {
			/* call failure; fail the connection attempt */
			lua_pop(L, 1); /* pcall will have pushed an error message */
			return -1;
		}
		if (lua_isnil(L, -1) || (lua_isboolean(L,-1) && lua_toboolean(L,-1) == false)) {
			/* nil or false return; stop iteration */
			lua_pop(L, 1);
			break;
		}
		size_t resl;
		const char *res = lua_tolstring(L, -1, &resl);
		if (res == NULL) {
			/* conversion failure; fail the connection attempt */
			lua_pop(L, 1);
			return -1;
		}
		if (!espconn_mbedtls_parse(msg, auth_type, res, resl+1)) {
			/* parsing failure; fail the connction attempt */
			lua_pop(L, 1);
			return -1;
		}

		/*
		 * Otherwise, parsing successful; if this is a loopy kind of
		 * callback, then increment the argument and loop.
		 */
		lua_pop(L, 1);
		cbarg++;
	} while (loop);

	return 1;
}

static bool mbedtls_msg_info_load(mbedtls_msg *msg, mbedtls_auth_type auth_type)
{
	const char* const begin = "-----BEGIN";
	const char* const type_name  = "private_key";
#define FILE_OFFSET	4
	int ret = 0;
	int32 offerset = 0;
	uint8* load_buf = NULL;
	size_t load_len = 0;
	file_param file_param;

	bzero(&file_param, sizeof(file_param));

again:
	espconn_ssl_read_param_from_flash(&file_param.file_head, sizeof(file_head), offerset, auth_type);
	file_param.file_offerset = offerset;
	os_printf("%s %d, type[%s],length[%d]\n", __FILE__, __LINE__, file_param.file_head.file_name, file_param.file_head.file_length);
	if (file_param.file_head.file_length == 0xFFFF) {
		return false;
	} else {
		/*Optional is load the private key*/
		if (auth_type == ESPCONN_PK && os_memcmp(&file_param.file_head.file_name, type_name, os_strlen(type_name)) != 0) {
			offerset += sizeof(file_head) + file_param.file_head.file_length;
			goto again;
		}
		/*Optional is load the cert*/
		if (auth_type == ESPCONN_CERT_OWN && os_memcmp(file_param.file_head.file_name, "certificate", os_strlen("certificate")) != 0) {
			offerset += sizeof(file_head) + file_param.file_head.file_length;
			goto again;
		}
		load_buf = (uint8_t *) os_zalloc( file_param.file_head.file_length + FILE_OFFSET);
		if (load_buf == NULL) {
			return false;
		}
		offerset = sizeof(file_head) + file_param.file_offerset;
		espconn_ssl_read_param_from_flash(load_buf,	file_param.file_head.file_length, offerset, auth_type);
	}

	load_len = file_param.file_head.file_length;
	/*
	* Determine buffer content. Buffer contains either one DER certificate or
	* one or more PEM certificates.
	*/
	if ((char*)os_strstr(load_buf, begin) != NULL) {
		load_len += 1;
		load_buf[load_len - 1] = '\0';
	}

	ret = espconn_mbedtls_parse(msg, auth_type, load_buf, load_len) ? 0 : -1;

exit:
	os_free(load_buf);
	if (ret < 0) {
		return false;
	} else {
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
	bool load_flag = false;
	int ret = ESPCONN_OK;

	/* Load upstream default configs */
	ret = mbedtls_ssl_config_defaults(&msg->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
	lwIP_REQUIRE_NOERROR(ret, exit);

	ret = mbedtls_ssl_setup(&msg->ssl, &msg->conf);
	lwIP_REQUIRE_NOERROR(ret, exit);

	/*Initialize the RNG and the session data*/
	ret = mbedtls_ctr_drbg_seed(&msg->ctr_drbg, mbedtls_entropy_func, &msg->entropy, "client", 6);
	lwIP_REQUIRE_NOERROR(ret, exit);

	/*Load the certificate and private RSA key*/
	ret = 0;
	if (ssl_client_options.cert_auth_callback != LUA_NOREF) {
		ret = nodemcu_tls_cert_get(msg, ESPCONN_PK);
		switch(ret) {
			case 0: break;
			case -1: ret = ESPCONN_ABRT; goto exit;
			case 1: switch(nodemcu_tls_cert_get(msg, ESPCONN_CERT_OWN)) {
				case -1: ret = ESPCONN_ABRT; goto exit;
				case 0: break;
				case 1:
					ret = mbedtls_ssl_conf_own_cert(&msg->conf, &msg->psession->clicert, &msg->psession->pkey);
					lwIP_REQUIRE_ACTION(ret == 0, exit, ret = ESPCONN_ABRT);
			}
		}
	}
	if (ret == 0 && ssl_client_options.cert_req_sector.flag) {
		load_flag = mbedtls_msg_info_load(msg, ESPCONN_CERT_OWN);
		lwIP_REQUIRE_ACTION(load_flag, exit, ret = ESPCONN_MEM);
		load_flag = mbedtls_msg_info_load(msg, ESPCONN_PK);
		lwIP_REQUIRE_ACTION(load_flag, exit, ret = ESPCONN_MEM);
		ret = mbedtls_ssl_conf_own_cert(&msg->conf, &msg->psession->clicert, &msg->psession->pkey);
		lwIP_REQUIRE_ACTION(ret == 0, exit, ret = ESPCONN_ABRT);
	}

	ret = 0;

	/*Load the trusted CA*/

	if (ssl_client_options.cert_verify_callback != LUA_NOREF) {
		ret = nodemcu_tls_cert_get(msg, ESPCONN_CERT_AUTH);
	   switch(ret) {
			case 0: break;
			case -1: ret = ESPCONN_ABRT; goto exit;
			case 1:
				mbedtls_ssl_conf_authmode(&msg->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
				mbedtls_ssl_conf_ca_chain(&msg->conf, &msg->psession->cacert, NULL);
				break;
		}
	}
	if(ret == 0 && ssl_client_options.cert_ca_sector.flag) {
		load_flag = mbedtls_msg_info_load(msg, ESPCONN_CERT_AUTH);
		lwIP_REQUIRE_ACTION(load_flag, exit, ret = ESPCONN_MEM);
		mbedtls_ssl_conf_authmode(&msg->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
		mbedtls_ssl_conf_ca_chain(&msg->conf, &msg->psession->cacert, NULL);
	} else if (ret == 0) {
		/*
		 * OPTIONAL is not optimal for security, but makes interop easier in this session
		 * This gets overridden below if appropriate.
		 */
		mbedtls_ssl_conf_authmode(&msg->conf, MBEDTLS_SSL_VERIFY_NONE);
	}

	ret = 0;

	mbedtls_ssl_conf_rng(&msg->conf, mbedtls_ctr_drbg_random, &msg->ctr_drbg);
	mbedtls_ssl_conf_dbg(&msg->conf, mbedtls_dbg, NULL);

	mbedtls_ssl_set_bio(&msg->ssl, &msg->fd, mbedtls_net_send, mbedtls_net_recv, NULL);

exit:
	if (ret != 0) {
		return false;
	} else {
		return true;
	}
}

int espconn_mbedtls_parse_internal(int socket, sint8 error)
{
	int ret = ERR_OK;
	bool config_flag = false;
	espconn_msg *Threadmsg = NULL;
	pmbedtls_msg TLSmsg = NULL;
	Threadmsg = mbedtls_msg_find(socket);
	lwIP_REQUIRE_ACTION(Threadmsg, exit, ret = ERR_MEM);
	TLSmsg = Threadmsg->pssl;
	lwIP_REQUIRE_ACTION(TLSmsg, exit, ret = ERR_MEM);

	if (error == ERR_OK) {
		if (TLSmsg->quiet) {
			uint8 *TheadBuff = NULL;
			size_t ThreadLen = MBEDTLS_SSL_PLAIN_ADD;
			TheadBuff = (uint8 *)os_zalloc(ThreadLen + 1);
			lwIP_REQUIRE_ACTION(TheadBuff, exit, ret = ERR_MEM);
			do {
				os_memset(TheadBuff, 0, ThreadLen);
				ret = mbedtls_ssl_read(&TLSmsg->ssl, TheadBuff, ThreadLen);
				if (ret > 0) {
					ESPCONN_EVENT_RECV(Threadmsg->pespconn, TheadBuff, ret);
				} else {
					if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == 0) {
						ret = ESPCONN_OK;
						break;
					} else if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
						ret = ESPCONN_OK;
						mbedtls_ssl_close_notify(&TLSmsg->ssl);
					} else {
						break;
					}
				}
			} while(1);
			os_free(TheadBuff);
			TheadBuff = NULL;
			lwIP_REQUIRE_NOERROR(ret, exit);
		} else {
			if (TLSmsg->ssl.state == MBEDTLS_SSL_HELLO_REQUEST) {
				os_printf("client handshake start.\n");
				config_flag = mbedtls_msg_config(TLSmsg);
				if (config_flag) {
//					mbedtls_keep_alive(TLSmsg->fd.fd, 1, SSL_KEEP_IDLE, SSL_KEEP_INTVL, SSL_KEEP_CNT);
					system_overclock();
				} else {
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
				} else {
					break;
				}
			}
			system_soft_wdt_restart();
			system_update_cpu_freq(cpu_freq);
			lwIP_REQUIRE_NOERROR(ret, exit);
			/**/
			TLSmsg->quiet = mbedtls_handshake_result(TLSmsg);
			if (TLSmsg->quiet) {
				os_printf("client handshake ok!\n");
//				mbedtls_keep_alive(TLSmsg->fd.fd, 0, SSL_KEEP_IDLE, SSL_KEEP_INTVL, SSL_KEEP_CNT);
				mbedtls_session_free(&TLSmsg->psession);
				mbedtls_handshake_succ(&TLSmsg->ssl);
				system_restoreclock();

				TLSmsg->SentFnFlag = true;
				ESPCONN_EVENT_CONNECTED(Threadmsg->pespconn);
			} else {
				lwIP_REQUIRE_NOERROR_ACTION(TLSmsg->verify_result, exit, ret = TLSmsg->verify_result);
			}
		}
	} else if (error < 0) {
		Threadmsg->pcommon.err = error;
		Threadmsg->pespconn->state = ESPCONN_CLOSE;
		mbedtls_net_free(&TLSmsg->fd);
		ets_post(lwIPThreadPrio, NETCONN_EVENT_ERROR, (uint32)Threadmsg);
	} else {
		ret = MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
		lwIP_REQUIRE_NOERROR(ret, exit);
	}

exit:
	if (ret != ESPCONN_OK) {
		mbedtls_fail_info(Threadmsg, ret);
		if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			Threadmsg->hs_status = ESPCONN_OK;
		}
		ets_post(lwIPThreadPrio, NETCONN_EVENT_CLOSE,(uint32)Threadmsg);
	}
	return ret;
}

int espconn_mbedtls_parse_thread(int socket, int event, int error)
{
	int ret = ERR_OK;
	espconn_msg *Threadmsg = NULL;
	pmbedtls_msg TLSmsg = NULL;
	Threadmsg = mbedtls_msg_find(socket);
	lwIP_REQUIRE_ACTION(Threadmsg, exit, ret = ERR_MEM);
	TLSmsg = Threadmsg->pssl;
	lwIP_REQUIRE_ACTION(TLSmsg, exit, ret = ERR_MEM);
	if (TLSmsg->quiet) {
		int	out_msglen = TLSmsg->ssl.out_msglen + 5;
		if (Threadmsg->pcommon.write_flag)
			TLSmsg->record_len += error;

		if (TLSmsg->record_len == out_msglen) {
			TLSmsg->record_len = 0;
			Threadmsg->pcommon.write_flag = false;
			if (Threadmsg->pcommon.cntr != 0) {
				espconn_ssl_sent(Threadmsg, Threadmsg->pcommon.ptrbuf, Threadmsg->pcommon.cntr);
			} else {
				TLSmsg->SentFnFlag = true;
				ESPCONN_EVENT_SEND(Threadmsg->pespconn);
			}
		}
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
	for (ListMsg = plink_active; ListMsg != NULL; ListMsg = ListMsg->pnext) {
		if (Threadmsg == ListMsg) {
			active_flag = true;
			break;
		}
	}

	if (active_flag) {
		/*remove the node from the active connection list*/
		espconn_list_delete(&plink_active, Threadmsg);
		mbedtls_msg_free(&TLSmsg);
		Threadmsg->pssl = NULL;

		switch (events->sig) {
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
	if (ret != ESPCONN_OK) {
		if (mbedTLSMsg != NULL)
			mbedtls_msg_free(&mbedTLSMsg);
		if (pclient != NULL)
			os_free(pclient);
	}
	return ret;
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

	if (length > MBEDTLS_SSL_PLAIN_ADD) {
		out_msglen = MBEDTLS_SSL_PLAIN_ADD;
	}

	Threadmsg->pcommon.write_flag = true;
	ret = mbedtls_ssl_write(&mbedTLSMsg->ssl, psent, out_msglen);
	if (ret > 0) {
		Threadmsg->pcommon.ptrbuf = psent + ret;
		Threadmsg->pcommon.cntr = length - ret;
	} else {
		if (ret == MBEDTLS_ERR_SSL_WANT_WRITE || ret == 0) {

		} else {
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

#endif
