/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Martin d'Allens <martin.dallens@gmail.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 */

/*
 * FIXME: sprintf->snprintf everywhere.
 * FIXME: support null characters in responses.
 */

#include "osapi.h"
#include "../libc/c_stdio.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "limits.h"
#include "httpclient.h"
#include "stdlib.h"
#include "pm/swtimer.h"

#define REDIRECTION_FOLLOW_MAX 20

/* Internal state. */
typedef struct request_args_t {
	char		* hostname;
	int		port;
#ifdef CLIENT_SSL_ENABLE
	bool		secure;
#endif
	char		* method;
	char		* path;
	char		* headers;
	char		* post_data;
	char		* buffer;
	int		buffer_size;
	int		redirect_follow_count;
	int		timeout;
	os_timer_t	timeout_timer;
	http_callback_t callback_handle;
} request_args_t;

static char * ICACHE_FLASH_ATTR esp_strdup( const char * str )
{
	if ( str == NULL )
	{
		return(NULL);
	}
	char * new_str = (char *) os_malloc( os_strlen( str ) + 1 ); /* 1 for null character */
	if ( new_str == NULL )
	{
		HTTPCLIENT_DEBUG( "esp_strdup: malloc error" );
		return(NULL);
	}
	os_strcpy( new_str, str );
	return(new_str);
}


static int ICACHE_FLASH_ATTR
esp_isupper( char c )
{
	return(c >= 'A' && c <= 'Z');
}


static int ICACHE_FLASH_ATTR
esp_isalpha( char c )
{
	return( (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') );
}


static int ICACHE_FLASH_ATTR
esp_isspace( char c )
{
	return(c == ' ' || c == '\t' || c == '\n' || c == '\12');
}


static int ICACHE_FLASH_ATTR
esp_isdigit( char c )
{
	return(c >= '0' && c <= '9');
}


static int ICACHE_FLASH_ATTR http_chunked_decode( const char * chunked, char * decode )
{
	int	i		= 0, j = 0;
	int	decode_size	= 0;
	char	*str		= (char *) chunked;
	do
	{
		char * endstr;
		/* [chunk-size] */
		i = strtoul( str + j, NULL, 16 );
		HTTPCLIENT_DEBUG( "Chunk Size:%d", i );
		if ( i <= 0 )
			break;
		/* [chunk-size-end-ptr] */
		endstr = (char *) os_strstr( str + j, "\r\n" );
		/* [chunk-ext] */
		j += endstr - (str + j);
		/* [CRLF] */
		j += 2;
		/* [chunk-data] */
		decode_size += i;
		os_memcpy( (char *) &decode[decode_size - i], (char *) str + j, i );
		j += i;
		/* [CRLF] */
		j += 2;
	}
	while ( true );

	/*
	 *
	 * footer CRLF
	 *
	 */

	return(j);
}


static void ICACHE_FLASH_ATTR http_receive_callback( void * arg, char * buf, unsigned short len )
{
	struct espconn	* conn	= (struct espconn *) arg;
	request_args_t	* req	= (request_args_t *) conn->reverse;

	if ( req->buffer == NULL )
	{
		return;
	}

	/* Let's do the equivalent of a realloc(). */
	const int	new_size = req->buffer_size + len;
	char		* new_buffer;
	if ( new_size > BUFFER_SIZE_MAX || NULL == (new_buffer = (char *) os_malloc( new_size ) ) )
	{
		HTTPCLIENT_ERR( "Response too long (%d)", new_size );
		req->buffer[0] = '\0';                                                                  /* Discard the buffer to avoid using an incomplete response. */
#ifdef CLIENT_SSL_ENABLE
		if ( req->secure )
			espconn_secure_disconnect( conn );
		else
#endif
			espconn_disconnect( conn );
		return;                                                                                 /* The disconnect callback will be called. */
	}

	os_memcpy( new_buffer, req->buffer, req->buffer_size );
	os_memcpy( new_buffer + req->buffer_size - 1 /*overwrite the null character*/, buf, len );      /* Append new data. */
	new_buffer[new_size - 1] = '\0';                                                                /* Make sure there is an end of string. */

	os_free( req->buffer );
	req->buffer		= new_buffer;
	req->buffer_size	= new_size;
}


static void ICACHE_FLASH_ATTR http_send_callback( void * arg )
{
	struct espconn	* conn	= (struct espconn *) arg;
	request_args_t	* req	= (request_args_t *) conn->reverse;

	if ( req->post_data == NULL )
	{
		HTTPCLIENT_DEBUG( "All sent" );
	}
	else  
	{
		/* The headers were sent, now send the contents. */
		HTTPCLIENT_DEBUG( "Sending request body" );
#ifdef CLIENT_SSL_ENABLE
		if ( req->secure )
			espconn_secure_send( conn, (uint8_t *) req->post_data, strlen( req->post_data ) );
		else
#endif
			espconn_send( conn, (uint8_t *) req->post_data, strlen( req->post_data ) );
		os_free( req->post_data );
		req->post_data = NULL;
	}
}


static void ICACHE_FLASH_ATTR http_connect_callback( void * arg )
{
	HTTPCLIENT_DEBUG( "Connected" );
	struct espconn	* conn	= (struct espconn *) arg;
	request_args_t	* req	= (request_args_t *) conn->reverse;
	espconn_regist_recvcb( conn, http_receive_callback );
	espconn_regist_sentcb( conn, http_send_callback );

	char post_headers[32] = "";

	if ( req->post_data != NULL ) /* If there is data then add Content-Length header. */
	{
		os_sprintf( post_headers, "Content-Length: %d\r\n", strlen( req->post_data ) );
	}

	if(req->headers == NULL) /* Avoid NULL pointer, it may cause exception */
	{
		req->headers = (char *)os_malloc(sizeof(char));
		req->headers[0] = '\0';
	}

	char ua_header[32] = "";
    int ua_len = 0;
    if (os_strstr( req->headers, "User-Agent:" ) == NULL && os_strstr( req->headers, "user-agent:" ) == NULL)
    {
        os_sprintf( ua_header, "User-Agent: %s\r\n", "ESP8266" );
        ua_len = strlen(ua_header);
    }

	char * host_header = "";
    int host_len = 0;
    if ( os_strstr( req->headers, "Host:" ) == NULL && os_strstr( req->headers, "host:" ) == NULL)
    {
        int max_header_len = 9 + strlen(req->hostname); // 9 is fixed size of "Host:[space][cr][lf]\0"
        if ((req->port == 80)
#ifdef CLIENT_SSL_ENABLE
            || ((req->port == 443) && ( req->secure ))
#endif
            )
        {
            host_header = alloca(max_header_len);
            os_sprintf( host_header, "Host: %s\r\n", req->hostname );
        }
        else
        {
            host_header = alloca(max_header_len + 6); // 6 is worst case of ":port" where port is maximum 5 digits
            os_sprintf( host_header, "Host: %s:%d\r\n", req->hostname, req->port );
        }
        host_len = strlen(host_header);
    }

    char buf[69 + strlen( req->method ) + strlen( req->path ) + host_len +
           strlen( req->headers ) + ua_len + strlen( post_headers )];
    int len = os_sprintf( buf,
            "%s %s HTTP/1.1\r\n"
            "%s" // Host (if not provided in the headers from Lua)
            "Connection: close\r\n"
            "%s" // Headers from Lua (optional)
            "%s" // User-Agent (if not provided in the headers from Lua)
            "%s" // Content-Length
            "\r\n",
            req->method, req->path, host_header, req->headers, ua_header, post_headers );

#ifdef CLIENT_SSL_ENABLE
    if (req->secure)
    {
        espconn_secure_send( conn, (uint8_t *) buf, len );
    }
    else
#endif
    {
        espconn_send( conn, (uint8_t *) buf, len );
    }

    if (req->headers != NULL)
    {
        os_free( req->headers );
    }

    req->headers = NULL;
    HTTPCLIENT_DEBUG( "Sending request header" );
}

static void http_free_req( request_args_t * req)
{
	if (req->buffer) {
		os_free( req->buffer );
	}
	if (req->post_data) {
		os_free( req->post_data );
	}
	if (req->headers) {
		os_free( req->headers );
	}
	os_free( req->hostname );
	os_free( req->method );
	os_free( req->path );
	os_free( req );
}

static void ICACHE_FLASH_ATTR http_disconnect_callback( void * arg )
{
	HTTPCLIENT_DEBUG( "Disconnected" );
	struct espconn *conn = (struct espconn *) arg;

	if ( conn == NULL )
	{
		return;
	}

	if ( conn->proto.tcp != NULL )
	{
		os_free( conn->proto.tcp );
	}
	if ( conn->reverse != NULL )
	{
		request_args_t	* req		= (request_args_t *) conn->reverse;
		int		http_status	= -1;
		char		* body		= "";

		// Turn off timeout timer
		os_timer_disarm( &(req->timeout_timer) );

		if ( req->buffer == NULL )
		{
			HTTPCLIENT_DEBUG( "Buffer probably shouldn't be NULL" );
		}
		else if ( req->buffer[0] != '\0' )
		{
			/* FIXME: make sure this is not a partial response, using the Content-Length header. */
			const char * version_1_0 = "HTTP/1.0 ";
			const char * version_1_1 = "HTTP/1.1 ";
			if (( os_strncmp( req->buffer, version_1_0, strlen( version_1_0 ) ) != 0 ) &&
				( os_strncmp( req->buffer, version_1_1, strlen( version_1_1 ) ) != 0 ))
			{
				HTTPCLIENT_ERR( "Invalid version in %s", req->buffer );
			}
			else  
			{
				http_status	= atoi( req->buffer + strlen( version_1_0 ) );

				char *locationOffset = (char *) os_strstr( req->buffer, "Location:" );
				if ( locationOffset == NULL ) {
					locationOffset = (char *) os_strstr( req->buffer, "location:" );
				}

				if ( locationOffset != NULL && http_status >= 300 && http_status <= 308 ) {
					if (req->redirect_follow_count < REDIRECTION_FOLLOW_MAX) {
						locationOffset += strlen("location:");

						while (*locationOffset == ' ') { // skip url leading white-space
							locationOffset++;
						}

						char *locationOffsetEnd = (char *) os_strstr(locationOffset, "\r\n");
						if ( locationOffsetEnd == NULL ) {
							HTTPCLIENT_ERR( "Found Location header but was incomplete" );
							http_status = -1;
						} else {
							*locationOffsetEnd = '\0';
							req->redirect_follow_count++;

							// Check if url is absolute
							bool url_has_protocol =
								os_strncmp( locationOffset, "http://", strlen( "http://" ) ) == 0 ||
								os_strncmp( locationOffset, "https://", strlen( "https://" ) ) == 0;

							if ( url_has_protocol ) {
								http_request( locationOffset, req->method, req->headers,
									req->post_data, req->callback_handle, req->redirect_follow_count );
							} else {
								if ( os_strncmp( locationOffset, "/", 1 ) == 0) { // relative and full path
									http_raw_request( req->hostname, req->port,
#ifdef CLIENT_SSL_ENABLE
									                  req->secure,
#else
									                  0,
#endif
									                  req->method, locationOffset, req->headers, req->post_data, req->callback_handle, req->redirect_follow_count );
								} else { // relative and relative path

									// find last /
									const char *pathFolderEnd = strrchr(req->path, '/');

									int pathFolderLength = pathFolderEnd - req->path;
									pathFolderLength++; // use the '/'
									int locationLength = strlen(locationOffset);
									locationLength++; // use the '\0'

									// append pathFolder with given relative path
									char *completeRelativePath = (char *) os_malloc(pathFolderLength + locationLength);
									os_memcpy( completeRelativePath, req->path, pathFolderLength );
									os_memcpy( completeRelativePath + pathFolderLength, locationOffset, locationLength);

									http_raw_request( req->hostname, req->port,
#ifdef CLIENT_SSL_ENABLE
									                  req->secure,
#else
									                  0,
#endif
									                  req->method, completeRelativePath, req->headers, req->post_data, req->callback_handle, req->redirect_follow_count );

									os_free( completeRelativePath );
								}
							}
							http_free_req( req );
							espconn_delete( conn );
							os_free( conn );
							return;
						}
					} else {
						HTTPCLIENT_ERR("Too many redirections");
						http_status = -1;
					}
				} else {
					body = (char *) os_strstr(req->buffer, "\r\n\r\n");

					if (NULL == body) {
						  /* Find missing body */
						  HTTPCLIENT_ERR("Body shouldn't be NULL");
						  /* To avoid NULL body */
						  body = "";
					} else {
						  /* Skip CR & LF */
						  body = body + 4;
					}

					if ( os_strstr( req->buffer, "Transfer-Encoding: chunked" ) || os_strstr( req->buffer, "transfer-encoding: chunked" ) )
					{
						int	body_size = req->buffer_size - (body - req->buffer);
						char	chunked_decode_buffer[body_size];
						os_memset( chunked_decode_buffer, 0, body_size );
						/* Chuncked data */
						http_chunked_decode( body, chunked_decode_buffer );
						os_memcpy( body, chunked_decode_buffer, body_size );
					}
				}
			}
		}
		if ( req->callback_handle != NULL ) /* Callback is optional. */
		{
                  char *req_buffer = req->buffer;
                  req->buffer = NULL;
                  http_callback_t req_callback;
                  req_callback = req->callback_handle;

		  http_free_req( req );

                  req_callback( body, http_status, &req_buffer );
                  if (req_buffer) {
                    os_free(req_buffer);
                  }
		} else {
		  http_free_req( req );
                }
	}
	/* Fix memory leak. */
	espconn_delete( conn );
	os_free( conn );
}


static void ICACHE_FLASH_ATTR http_timeout_callback( void *arg )
{
	HTTPCLIENT_ERR( "Connection timeout" );
	struct espconn * conn = (struct espconn *) arg;
	if ( conn == NULL )
	{
		HTTPCLIENT_DEBUG( "Connection is NULL" );
		return;
	}
	if ( conn->reverse == NULL )
	{
		HTTPCLIENT_DEBUG( "Connection request data (reverse) is NULL" );
		return;
	}
	request_args_t * req = (request_args_t *) conn->reverse;
	HTTPCLIENT_DEBUG( "Calling disconnect" );
	/* Call disconnect */
	sint8 result;
#ifdef CLIENT_SSL_ENABLE
	if ( req->secure )
		result = espconn_secure_disconnect( conn );
	else
#endif
		result = espconn_disconnect( conn );
		
	if (result == ESPCONN_OK || result == ESPCONN_INPROGRESS)
		return;
	else
	{
		/* not connected; execute the callback ourselves. */
		HTTPCLIENT_DEBUG( "manually Calling disconnect callback due to error %d", result );
		http_disconnect_callback( arg );
	}		
}


static void ICACHE_FLASH_ATTR http_error_callback( void *arg, sint8 errType )
{
	HTTPCLIENT_ERR( "Disconnected with error: %d", errType );
	http_timeout_callback( arg );
}


static void ICACHE_FLASH_ATTR http_dns_callback( const char * hostname, ip_addr_t * addr, void * arg )
{
	request_args_t * req = (request_args_t *) arg;

	if ( addr == NULL )
	{
		HTTPCLIENT_ERR( "DNS failed for %s", hostname );
		if ( req->callback_handle != NULL )
		{
			req->callback_handle( "", -1, NULL );
		}
		http_free_req( req );
	}
	else  
	{
		HTTPCLIENT_DEBUG( "DNS found %s " IPSTR, hostname, IP2STR( addr ) );

		struct espconn * conn = (struct espconn *) os_zalloc( sizeof(struct espconn) );
		conn->type			= ESPCONN_TCP;
		conn->state			= ESPCONN_NONE;
		conn->proto.tcp			= (esp_tcp *) os_zalloc( sizeof(esp_tcp) );
		conn->proto.tcp->local_port	= espconn_port();
		conn->proto.tcp->remote_port	= req->port;
		conn->reverse			= req;

		os_memcpy( conn->proto.tcp->remote_ip, addr, 4 );

		espconn_regist_connectcb( conn, http_connect_callback );
		espconn_regist_disconcb( conn, http_disconnect_callback );
		espconn_regist_reconcb( conn, http_error_callback );

		/* Set connection timeout timer */
		os_timer_disarm( &(req->timeout_timer) );
		os_timer_setfn( &(req->timeout_timer), (os_timer_func_t *) http_timeout_callback, conn );
		SWTIMER_REG_CB(http_timeout_callback, SWTIMER_IMMEDIATE);
		  //http_timeout_callback frees memory used by this function and timer cannot be dropped
		os_timer_arm( &(req->timeout_timer), req->timeout, false );

#ifdef CLIENT_SSL_ENABLE
		if ( req->secure )
		{
			espconn_secure_connect( conn );
		} 
		else 
#endif
		{
			espconn_connect( conn );
		}
	}
}


void ICACHE_FLASH_ATTR http_raw_request( const char * hostname, int port, bool secure, const char * method, const char * path, const char * headers, const char * post_data, http_callback_t callback_handle, int redirect_follow_count )
{
	HTTPCLIENT_DEBUG( "DNS request" );

	request_args_t * req = (request_args_t *) os_zalloc( sizeof(request_args_t) );
	req->hostname		= esp_strdup( hostname );
	req->port		= port;
#ifdef CLIENT_SSL_ENABLE
	req->secure		= secure;
#endif
	req->method		= esp_strdup( method );
	req->path		= esp_strdup( path );
	req->headers		= esp_strdup( headers );
	req->post_data		= esp_strdup( post_data );
	req->buffer_size	= 1;
	req->buffer		= (char *) os_malloc( 1 );
	req->buffer[0]		= '\0';                                         /* Empty string. */
	req->callback_handle	= callback_handle;
	req->timeout		= HTTP_REQUEST_TIMEOUT_MS;
	req->redirect_follow_count = redirect_follow_count;

	ip_addr_t	addr;
	err_t		error = espconn_gethostbyname( (struct espconn *) req,  /* It seems we don't need a real espconn pointer here. */
						       hostname, &addr, http_dns_callback );

	if ( error == ESPCONN_INPROGRESS )
	{
		HTTPCLIENT_DEBUG( "DNS pending" );
	}
	else if ( error == ESPCONN_OK )
	{
		/* Already in the local names table (or hostname was an IP address), execute the callback ourselves. */
		http_dns_callback( hostname, &addr, req );
	}
	else  
	{
		if ( error == ESPCONN_ARG )
		{
			HTTPCLIENT_ERR( "DNS arg error %s", hostname );
		}else  {
			HTTPCLIENT_ERR( "DNS error code %d", error );
		}
		http_dns_callback( hostname, NULL, req ); /* Handle all DNS errors the same way. */
	}
}


/*
 * Parse an URL of the form http://host:port/path
 * <host> can be a hostname or an IP address
 * <port> is optional
 */
void ICACHE_FLASH_ATTR http_request( const char * url, const char * method, const char * headers, const char * post_data, http_callback_t callback_handle, int redirect_follow_count )
{
	/*
	 * FIXME: handle HTTP auth with http://user:pass@host/
	 * FIXME: get rid of the #anchor part if present.
	 */

	char	hostname[128]	= "";
	int	port		= 80;
	bool	secure		= false;

	bool	is_http		= os_strncmp( url, "http://", strlen( "http://" ) ) == 0;
	bool	is_https	= os_strncmp( url, "https://", strlen( "https://" ) ) == 0;

	if ( is_http )
		url += strlen( "http://" );             /* Get rid of the protocol. */
	else if ( is_https )
	{
		port	= 443;
		secure	= true;
		url	+= strlen( "https://" );        /* Get rid of the protocol. */
	} 
	else 
	{
		HTTPCLIENT_ERR( "URL is not HTTP or HTTPS %s", url );
		return;
	}

	char * path = os_strchr( url, '/' );
	if ( path == NULL )
	{
		path = os_strchr( url, '\0' );  /* Pointer to end of string. */
	}

	char * colon = os_strchr( url, ':' );
	if ( colon > path )
	{
		colon = NULL;                   /* Limit the search to characters before the path. */
	}

	if (path - url >= sizeof(hostname)) {
		HTTPCLIENT_ERR( "hostname is too long %s", url );
		return;
	}

	if ( colon == NULL )                    /* The port is not present. */
	{
		os_memcpy( hostname, url, path - url );
		hostname[path - url] = '\0';
	}
	else  
	{
		port = atoi( colon + 1 );
		if ( port == 0 )
		{
			HTTPCLIENT_ERR( "Port error %s", url );
			return;
		}

		os_memcpy( hostname, url, colon - url );
		hostname[colon - url] = '\0';
	}


	if ( path[0] == '\0' ) /* Empty path is not allowed. */
	{
		path = "/";
	}

	HTTPCLIENT_DEBUG( "hostname=%s", hostname );
	HTTPCLIENT_DEBUG( "port=%d", port );
	HTTPCLIENT_DEBUG( "method=%s", method );
	HTTPCLIENT_DEBUG( "path=%s", path );
	http_raw_request( hostname, port, secure, method, path, headers, post_data, callback_handle, redirect_follow_count);
}


/*
 * Parse an URL of the form http://host:port/path
 * <host> can be a hostname or an IP address
 * <port> is optional
 */
void ICACHE_FLASH_ATTR http_post( const char * url, const char * headers, const char * post_data, http_callback_t callback_handle )
{
	http_request( url, "POST", headers, post_data, callback_handle, 0 );
}


void ICACHE_FLASH_ATTR http_get( const char * url, const char * headers, http_callback_t callback_handle )
{
	http_request( url, "GET", headers, NULL, callback_handle, 0 );
}


void ICACHE_FLASH_ATTR http_delete( const char * url, const char * headers, const char * post_data, http_callback_t callback_handle )
{
	http_request( url, "DELETE", headers, post_data, callback_handle, 0 );
}


void ICACHE_FLASH_ATTR http_put( const char * url, const char * headers, const char * post_data, http_callback_t callback_handle )
{
	http_request( url, "PUT", headers, post_data, callback_handle, 0 );
}


void ICACHE_FLASH_ATTR http_callback_example( char * response, int http_status, char * full_response )
{
	dbg_printf( "http_status=%d\n", http_status );
	if ( http_status != HTTP_STATUS_GENERIC_ERROR )
	{
		dbg_printf( "strlen(full_response)=%d\n", strlen( full_response ) );
		dbg_printf( "response=%s<EOF>\n", response );
	}
}

