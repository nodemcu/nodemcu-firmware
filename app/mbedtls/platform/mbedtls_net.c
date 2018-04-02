/*
 *  TCP/IP or UDP/IP networking functions
 *  modified for LWIP support on ESP8266
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  Additions Copyright (C) 2015 Angus Gratton
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */
#include "mbedtls/config.h"

#if defined(MBEDTLS_NET_C)

#include "mbedtls/net_sockets.h"

#include <string.h>
//
#include <sys/types.h>
#include <sys/socket.h>
//#include <sys/time.h>
#include <unistd.h>
#include "lwip/netdb.h"
#include <errno.h>
//
#include <stdlib.h>
#include <stdio.h>
//
#include <time.h>
//
#include <stdint.h>

/*
 * Prepare for using the sockets interface
 */
static int net_prepare( void )
{
#if ( defined(_WIN32) || defined(_WIN32_WCE) ) && !defined(EFIX64) && \
    !defined(EFI32)
    WSADATA wsaData;

    if( wsa_init_done == 0 )
    {
        if( WSAStartup( MAKEWORD(2,0), &wsaData ) != 0 )
            return( MBEDTLS_ERR_NET_SOCKET_FAILED );

        wsa_init_done = 1;
    }
#else
#endif
    return( 0 );
}

/*
 * Initialize a context
 */
void mbedtls_net_init( mbedtls_net_context *ctx )
{
    ctx->fd = -1;
}

/*
 * Initiate a TCP connection with host:port and the given protocol
 */
int mbedtls_net_connect( mbedtls_net_context *ctx, const char *host, const char *port, int proto )
{
    int ret;
    uint32 ports = getul((char*)port);
    struct sockaddr_in client_addr;
    struct sockaddr_in server_addr;

    if( ( ret = net_prepare() ) != 0 )
        return( ret );

    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htons(IPADDR_ANY);
    client_addr.sin_port = htons(0);

    ctx->fd = socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
	if( ctx->fd < 0 ){
		ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
		os_printf("Create Socket Failed!\n");
		goto mbedtls_error;
	}

	if (bind(ctx->fd, (struct sockaddr*)&client_addr,sizeof(client_addr))) {
		ret = MBEDTLS_ERR_NET_BIND_FAILED;
		os_printf("Client Bind Port Failed!\n");
		goto mbedtls_error;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = ipaddr_addr(host);
	server_addr.sin_port = htons(ports);

	ret = connect(ctx->fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret < 0){
		ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
		os_printf("Can Not Connect To!\n");
	}

mbedtls_error:
	if (ret < 0)
		close( ctx->fd );
    return( ret );
}

/*
 * Create a listening socket on bind_ip:port
 */
int mbedtls_net_bind( mbedtls_net_context *ctx, const char *bind_ip, const char *port, int proto )
{
	int ret;
	int protocol = 0;
	int sock_type = 0;
	uint32 ports = getul((char*)port);

	struct sockaddr_in server_addr;

    if( ( ret = net_prepare() ) != 0 )
        return( ret );

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htons(IPADDR_ANY);
    server_addr.sin_port = htons(ports);
    protocol = proto == MBEDTLS_NET_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP;
    sock_type = proto == MBEDTLS_NET_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM;

    ctx->fd = socket(AF_INET, sock_type, protocol);
    if( ctx->fd < 0 ){
		ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
		os_printf("Create Socket Failed!\n");
		goto mbedtls_error;
	}

    ret =  bind(ctx->fd, (struct sockaddr*)&server_addr,sizeof(server_addr));
    if (ret != 0){
       ret = MBEDTLS_ERR_NET_BIND_FAILED;
       os_printf("Server Bind Port Failed!\n");
       goto mbedtls_error;
    }

    if (proto == MBEDTLS_NET_PROTO_TCP){
    	ret = listen(ctx->fd, MBEDTLS_ERR_NET_LISTEN_FAILED);
    	if (ret != 0){
    		ret = MBEDTLS_ERR_NET_LISTEN_FAILED;
    		goto mbedtls_error;
    	}
    }

    ret = 0;

    mbedtls_error:
    	if (ret < 0)
    		close( ctx->fd );
        return( ret );

}

#if ( defined(_WIN32) || defined(_WIN32_WCE) ) && !defined(EFIX64) && \
    !defined(EFI32)
/*
 * Check if the requested operation would be blocking on a non-blocking socket
 * and thus 'failed' with a negative return value.
 */
static int net_would_block( const mbedtls_net_context *ctx )
{
    ((void) ctx);
    return( WSAGetLastError() == WSAEWOULDBLOCK );
}
#else
/*
 * Check if the requested operation would be blocking on a non-blocking socket
 * and thus 'failed' with a negative return value.
 *
 * Note: on a blocking socket this function always returns 0!
 */
static int net_would_block( const mbedtls_net_context *ctx )
{
    /*
     * Never return 'WOULD BLOCK' on a non-blocking socket
     */
//    if( ( fcntl( ctx->fd, F_GETFL, 0) & O_NONBLOCK ) != O_NONBLOCK )
//        return( 0 );

//    switch( errno )
//    {
//#if defined EAGAIN
//        case EAGAIN:
//#endif
//#if defined EWOULDBLOCK && EWOULDBLOCK != EAGAIN
//        case EWOULDBLOCK:
//#endif
//            return( 1 );
//    }
    return( 0 );
}
#endif /* ( _WIN32 || _WIN32_WCE ) && !EFIX64 && !EFI32 */

/*
 * Accept a connection from a remote client
 */
int mbedtls_net_accept( mbedtls_net_context *bind_ctx,
                        mbedtls_net_context *client_ctx,
                        void *client_ip, size_t buf_size, size_t *ip_len )
{
    int ret;
    int type;

    struct sockaddr_in client_addr;

    socklen_t n = (socklen_t) sizeof( client_addr );
    socklen_t type_len = (socklen_t) sizeof( type );

	client_ctx->fd = (int) accept( bind_ctx->fd,(struct sockaddr *) &client_addr, &n );
	if (client_ctx->fd < 0){
		ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
		os_printf("Create Socket Failed!\n");
		goto mbedtls_error;
	}

	if(client_ip != NULL){
		struct sockaddr_in *addr4 = (struct sockaddr_in *) &client_addr;
		*ip_len = sizeof( addr4->sin_addr.s_addr );

		if( buf_size < *ip_len ){
			ret = MBEDTLS_ERR_NET_BUFFER_TOO_SMALL;
			os_printf("Create Socket Failed!\n");
			goto mbedtls_error;
		}

		memcpy(client_ip, &addr4->sin_addr.s_addr, *ip_len);
	}
	ret = 0;
	mbedtls_error:
	    	if (ret < 0)
	    		close( client_ctx->fd );
	        return( ret );
    return( 0 );
}

/*
 * Set the socket blocking or non-blocking
 */
int mbedtls_net_set_block( mbedtls_net_context *ctx )
{
#if ( defined(_WIN32) || defined(_WIN32_WCE) ) && !defined(EFIX64) && \
    !defined(EFI32)
    u_long n = 0;
    return( ioctlsocket( ctx->fd, FIONBIO, &n ) );
#else
    return( fcntl( ctx->fd, F_SETFL, fcntl( ctx->fd, F_GETFL, 0 ) & ~O_NONBLOCK ) );
#endif
}

int mbedtls_net_set_nonblock( mbedtls_net_context *ctx )
{
#if ( defined(_WIN32) || defined(_WIN32_WCE) ) && !defined(EFIX64) && \
    !defined(EFI32)
    u_long n = 1;
    return( ioctlsocket( ctx->fd, FIONBIO, &n ) );
#else
    return( fcntl( ctx->fd, F_SETFL, fcntl( ctx->fd, F_GETFL, 0 ) | O_NONBLOCK ) );
#endif
}

/*
 * Portable usleep helper
 */
void mbedtls_net_usleep( unsigned long usec )
{
//#if defined(_WIN32)
//    Sleep( ( usec + 999 ) / 1000 );
//#else
//    struct timeval tv;
//    tv.tv_sec  = usec / 1000000;
//#if defined(__unix__) || defined(__unix) || \
//    ( defined(__APPLE__) && defined(__MACH__) )
//    tv.tv_usec = (suseconds_t) usec % 1000000;
//#else
//    tv.tv_usec = usec % 1000000;
//#endif
//    select( 0, NULL, NULL, NULL, &tv );
//#endif
}

/*
 * Read at most 'len' characters
 */
int mbedtls_net_recv( void *ctx, unsigned char *buf, size_t len )
{
    int ret;
    int fd = ((mbedtls_net_context *) ctx)->fd;

    if( fd < 0 )
        return( MBEDTLS_ERR_NET_INVALID_CONTEXT );

//    os_printf("mbedtls_net_recv need %d\n", len);
    ret = (int) read( fd, buf, len );

    if( ret < 0 )
    {
        if( net_would_block( ctx ) != 0 )
            return( MBEDTLS_ERR_SSL_WANT_READ );

//#if ( defined(_WIN32) || defined(_WIN32_WCE) ) && !defined(EFIX64) && \
//    !defined(EFI32)
//        if( WSAGetLastError() == WSAECONNRESET )
//            return( MBEDTLS_ERR_NET_CONN_RESET );
//#else
//        if( errno == EPIPE || errno == ECONNRESET )
//            return( MBEDTLS_ERR_NET_CONN_RESET );
//
//        if( errno == EINTR )
//            return( MBEDTLS_ERR_SSL_WANT_READ );
//#endif

        return( MBEDTLS_ERR_NET_RECV_FAILED );
    }
//    os_printf("mbedtls_net_recv get %d\n", ret);
    if (ret == 0)
    	return( MBEDTLS_ERR_SSL_WANT_READ );

    return( ret );
}

/*
 * Read at most 'len' characters, blocking for at most 'timeout' ms
 */
int mbedtls_net_recv_timeout( void *ctx, unsigned char *buf, size_t len,
                      uint32_t timeout )
{
    int ret;
    struct timeval tv;
    fd_set read_fds;
    int fd = ((mbedtls_net_context *) ctx)->fd;

    if( fd < 0 )
        return( MBEDTLS_ERR_NET_INVALID_CONTEXT );

//    FD_ZERO( &read_fds );
//    FD_SET( fd, &read_fds );
//
//    tv.tv_sec  = timeout / 1000;
//    tv.tv_usec = ( timeout % 1000 ) * 1000;
//
//    ret = select( fd + 1, &read_fds, NULL, NULL, timeout == 0 ? NULL : &tv );
//
//    /* Zero fds ready means we timed out */
//    if( ret == 0 )
//        return( MBEDTLS_ERR_SSL_TIMEOUT );
//
//    if( ret < 0 )
//    {
//#if ( defined(_WIN32) || defined(_WIN32_WCE) ) && !defined(EFIX64) && \
//    !defined(EFI32)
//        if( WSAGetLastError() == WSAEINTR )
//            return( MBEDTLS_ERR_SSL_WANT_READ );
//#else
//        if( errno == EINTR )
//            return( MBEDTLS_ERR_SSL_WANT_READ );
//#endif
//
//        return( MBEDTLS_ERR_NET_RECV_FAILED );
//    }

    /* This call will not block */
    return( mbedtls_net_recv( ctx, buf, len ) );
}

/*
 * Write at most 'len' characters
 */
int mbedtls_net_send( void *ctx, const unsigned char *buf, size_t len )
{
    int ret;
    int fd = ((mbedtls_net_context *) ctx)->fd;

    if( fd < 0 )
        return( MBEDTLS_ERR_NET_INVALID_CONTEXT );

//    os_printf("mbedtls_net_send need %d\n", len);
    ret = (int) write( fd, buf, len );

    if( ret < 0 )
    {
        if( net_would_block( ctx ) != 0 )
            return( MBEDTLS_ERR_SSL_WANT_WRITE );

//#if ( defined(_WIN32) || defined(_WIN32_WCE) ) && !defined(EFIX64) && \
//    !defined(EFI32)
//        if( WSAGetLastError() == WSAECONNRESET )
//            return( MBEDTLS_ERR_NET_CONN_RESET );
//#else
//        if( errno == EPIPE || errno == ECONNRESET )
//            return( MBEDTLS_ERR_NET_CONN_RESET );
//
//        if( errno == EINTR )
//            return( MBEDTLS_ERR_SSL_WANT_WRITE );
//#endif
//
//        return( MBEDTLS_ERR_NET_SEND_FAILED );
    }
//    os_printf("mbedtls_net_send write %d\n", ret);
    if (ret == 0)
        return( MBEDTLS_ERR_SSL_WANT_WRITE );
    return( ret );
}

/*
 * Gracefully close the connection
 */
void mbedtls_net_free( mbedtls_net_context *ctx )
{
    if( ctx->fd == -1 )
        return;

    close( ctx->fd );

    ctx->fd = -1;
}

#endif /* MBEDTLS_NET_C */
