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
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "limits.h"
#include "httpclient.h"

/* Internal state. */
typedef struct request_args_t {
	char		* hostname;
	int		port;
	bool		secure;
	char		* method;
	char		* path;
	char		* headers;
	char		* post_data;
	char		* buffer;
	int		buffer_size;
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


/*
 * Convert a string to a long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
long ICACHE_FLASH_ATTR
esp_strtol( nptr, endptr, base )
const char *nptr;


char	**endptr;
int	base;
{
	const char	*s = nptr;
	unsigned long	acc;
	int		c;
	unsigned long	cutoff;
	int		neg = 0, any, cutlim;


	/*
	 * Skip white space and pick up leading +/- sign if any.
	 * If base is 0, allow 0x for hex and 0 for octal, else
	 * assume decimal; if base is already 16, allow 0x.
	 */
	do
	{
		c = *s++;
	}
	while ( esp_isspace( c ) );
	if ( c == '-' )
	{
		neg	= 1;
		c	= *s++;
	} else if ( c == '+' )
		c = *s++;
	if ( (base == 0 || base == 16) &&
	     c == '0' && (*s == 'x' || *s == 'X') )
	{
		c	= s[1];
		s	+= 2;
		base	= 16;
	} else if ( (base == 0 || base == 2) &&
		    c == '0' && (*s == 'b' || *s == 'B') )
	{
		c	= s[1];
		s	+= 2;
		base	= 2;
	}
	if ( base == 0 )
		base = c == '0' ? 8 : 10;


	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for longs is
	 * [-2147483648..2147483647] and the input base is 10,
	 * cutoff will be set to 214748364 and cutlim to either
	 * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
	 * a value > 214748364, or equal but the next digit is > 7 (or 8),
	 * the number is too big, and we will return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	cutoff	= neg ? -(unsigned long) LONG_MIN : LONG_MAX;
	cutlim	= cutoff % (unsigned long) base;
	cutoff	/= (unsigned long) base;
	for ( acc = 0, any = 0;; c = *s++ )
	{
		if ( esp_isdigit( c ) )
			c -= '0';
		else if ( esp_isalpha( c ) )
			c -= esp_isupper( c ) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if ( c >= base )
			break;
		if ( any < 0 || acc > cutoff || acc == cutoff && c > cutlim )
			any = -1;
		else 
		{
			any	= 1;
			acc	*= base;
			acc	+= c;
		}
	}
	if ( any < 0 )
	{
		acc = neg ? LONG_MIN : LONG_MAX;
/*		errno = ERANGE; */
	} else if ( neg )
		acc = -acc;
	if ( endptr != 0 )
		*endptr = (char *) (any ? s - 1 : nptr);
	return(acc);
}

static int ICACHE_FLASH_ATTR http_chunked_decode( const char * chunked, char * decode )
{
	int	i		= 0, j = 0;
	int	decode_size	= 0;
	char	*str		= (char *) chunked;
	do
	{
		char * endstr = NULL;
		/* [chunk-size] */
		i = esp_strtol( str + j, endstr, 16 );
		HTTPCLIENT_DEBUG( "Chunk Size:%d\r\n", i );
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
		HTTPCLIENT_DEBUG( "Response too long (%d)\n", new_size );
		req->buffer[0] = '\0';                                                                  /* Discard the buffer to avoid using an incomplete response. */
		if ( req->secure )
			espconn_secure_disconnect( conn );
		else
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
		HTTPCLIENT_DEBUG( "All sent\n" );
	}
	else  
	{
		/* The headers were sent, now send the contents. */
		HTTPCLIENT_DEBUG( "Sending request body\n" );
		if ( req->secure )
			espconn_secure_send( conn, (uint8_t *) req->post_data, strlen( req->post_data ) );
		else
			espconn_send( conn, (uint8_t *) req->post_data, strlen( req->post_data ) );
		os_free( req->post_data );
		req->post_data = NULL;
	}
}


static void ICACHE_FLASH_ATTR http_connect_callback( void * arg )
{
	HTTPCLIENT_DEBUG( "Connected\n" );
	struct espconn	* conn	= (struct espconn *) arg;
	request_args_t	* req	= (request_args_t *) conn->reverse;

	espconn_regist_recvcb( conn, http_receive_callback );
	espconn_regist_sentcb( conn, http_send_callback );

	char post_headers[32] = "";

	if ( req->post_data != NULL ) /* If there is data then add Content-Length header. */
	{
		os_sprintf( post_headers, "Content-Length: %d\r\n", strlen( req->post_data ) );
	}

	char buf[69 + strlen( req->method ) + strlen( req->path ) + strlen( req->hostname ) +
		 strlen( req->headers ) + strlen( post_headers )];
	int len = os_sprintf( buf,
			      "%s %s HTTP/1.1\r\n"
			      "Host: %s:%d\r\n"
			      "Connection: close\r\n"
			      "User-Agent: ESP8266\r\n"
			      "%s"
			      "%s"
			      "\r\n",
			      req->method, req->path, req->hostname, req->port, req->headers, post_headers );

	if ( req->secure )
		espconn_secure_send( conn, (uint8_t *) buf, len );
	else
		espconn_send( conn, (uint8_t *) buf, len );
	os_free( req->headers );
	req->headers = NULL;
	HTTPCLIENT_DEBUG( "Sending request header\n" );
}


static void ICACHE_FLASH_ATTR http_disconnect_callback( void * arg )
{
	HTTPCLIENT_DEBUG( "Disconnected\n" );
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
			HTTPCLIENT_DEBUG( "Buffer shouldn't be NULL\n" );
		}
		else if ( req->buffer[0] != '\0' )
		{
			/* FIXME: make sure this is not a partial response, using the Content-Length header. */
			const char * version = "HTTP/1.1 ";
			if ( os_strncmp( req->buffer, version, strlen( version ) ) != 0 )
			{
				HTTPCLIENT_DEBUG( "Invalid version in %s\n", req->buffer );
			}
			else  
			{
				http_status	= atoi( req->buffer + strlen( version ) );
				body		= (char *) os_strstr( req->buffer, "\r\n\r\n" ) + 4;
				if ( os_strstr( req->buffer, "Transfer-Encoding: chunked" ) )
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
		if ( req->callback_handle != NULL ) /* Callback is optional. */
		{
			req->callback_handle( body, http_status, req->buffer );
		}
		os_free( req->buffer );
		os_free( req->hostname );
		os_free( req->method );
		os_free( req->path );
		os_free( req );
	}
	/* Fix memory leak. */
	espconn_delete( conn );
	os_free( conn );
}


static void ICACHE_FLASH_ATTR http_error_callback( void *arg, sint8 errType )
{
	HTTPCLIENT_DEBUG( "Disconnected with error\n" );
	http_disconnect_callback( arg );
}


static void ICACHE_FLASH_ATTR http_timeout_callback( void *arg )
{
	HTTPCLIENT_DEBUG( "Connection timeout\n" );
	struct espconn * conn = (struct espconn *) arg;
	if ( conn == NULL )
	{
		return;
	}
	if ( conn->reverse == NULL )
	{
		return;
	}
	request_args_t * req = (request_args_t *) conn->reverse;
	/* Call disconnect */
	if ( req->secure )
		espconn_secure_disconnect( conn );
	else
		espconn_disconnect( conn );
}


static void ICACHE_FLASH_ATTR http_dns_callback( const char * hostname, ip_addr_t * addr, void * arg )
{
	request_args_t * req = (request_args_t *) arg;

	if ( addr == NULL )
	{
		HTTPCLIENT_DEBUG( "DNS failed for %s\n", hostname );
		if ( req->callback_handle != NULL )
		{
			req->callback_handle( "", -1, "" );
		}
		os_free( req );
	}
	else  
	{
		HTTPCLIENT_DEBUG( "DNS found %s " IPSTR "\n", hostname, IP2STR( addr ) );

		struct espconn * conn = (struct espconn *) os_malloc( sizeof(struct espconn) );
		conn->type			= ESPCONN_TCP;
		conn->state			= ESPCONN_NONE;
		conn->proto.tcp			= (esp_tcp *) os_malloc( sizeof(esp_tcp) );
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
		os_timer_arm( &(req->timeout_timer), req->timeout, false );

		if ( req->secure )
		{
			espconn_secure_set_size( ESPCONN_CLIENT, 5120 ); /* set SSL buffer size */
			espconn_secure_connect( conn );
		} 
		else 
		{
			espconn_connect( conn );
		}
	}
}


void ICACHE_FLASH_ATTR http_raw_request( const char * hostname, int port, bool secure, const char * method, const char * path, const char * headers, const char * post_data, http_callback_t callback_handle )
{
	HTTPCLIENT_DEBUG( "DNS request\n" );

	request_args_t * req = (request_args_t *) os_malloc( sizeof(request_args_t) );
	req->hostname		= esp_strdup( hostname );
	req->port		= port;
	req->secure		= secure;
	req->method		= esp_strdup( method );
	req->path		= esp_strdup( path );
	req->headers		= esp_strdup( headers );
	req->post_data		= esp_strdup( post_data );
	req->buffer_size	= 1;
	req->buffer		= (char *) os_malloc( 1 );
	req->buffer[0]		= '\0';                                         /* Empty string. */
	req->callback_handle	= callback_handle;
	req->timeout		= HTTP_REQUEST_TIMEOUT_MS;

	ip_addr_t	addr;
	err_t		error = espconn_gethostbyname( (struct espconn *) req,  /* It seems we don't need a real espconn pointer here. */
						       hostname, &addr, http_dns_callback );

	if ( error == ESPCONN_INPROGRESS )
	{
		HTTPCLIENT_DEBUG( "DNS pending\n" );
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
			HTTPCLIENT_DEBUG( "DNS arg error %s\n", hostname );
		}else  {
			HTTPCLIENT_DEBUG( "DNS error code %d\n", error );
		}
		http_dns_callback( hostname, NULL, req ); /* Handle all DNS errors the same way. */
	}
}


/*
 * Parse an URL of the form http://host:port/path
 * <host> can be a hostname or an IP address
 * <port> is optional
 */
void ICACHE_FLASH_ATTR http_request( const char * url, const char * method, const char * headers, const char * post_data, http_callback_t callback_handle )
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
		HTTPCLIENT_DEBUG( "URL is not HTTP or HTTPS %s\n", url );
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
			HTTPCLIENT_DEBUG( "Port error %s\n", url );
			return;
		}

		os_memcpy( hostname, url, colon - url );
		hostname[colon - url] = '\0';
	}


	if ( path[0] == '\0' ) /* Empty path is not allowed. */
	{
		path = "/";
	}

	HTTPCLIENT_DEBUG( "hostname=%s\n", hostname );
	HTTPCLIENT_DEBUG( "port=%d\n", port );
	HTTPCLIENT_DEBUG( "method=%s\n", method );
	HTTPCLIENT_DEBUG( "path=%s\n", path );
	http_raw_request( hostname, port, secure, method, path, headers, post_data, callback_handle );
}


/*
 * Parse an URL of the form http://host:port/path
 * <host> can be a hostname or an IP address
 * <port> is optional
 */
void ICACHE_FLASH_ATTR http_post( const char * url, const char * headers, const char * post_data, http_callback_t callback_handle )
{
	http_request( url, "POST", headers, post_data, callback_handle );
}


void ICACHE_FLASH_ATTR http_get( const char * url, const char * headers, http_callback_t callback_handle )
{
	http_request( url, "GET", headers, NULL, callback_handle );
}


void ICACHE_FLASH_ATTR http_delete( const char * url, const char * headers, const char * post_data, http_callback_t callback_handle )
{
	http_request( url, "DELETE", headers, post_data, callback_handle );
}


void ICACHE_FLASH_ATTR http_put( const char * url, const char * headers, const char * post_data, http_callback_t callback_handle )
{
	http_request( url, "PUT", headers, post_data, callback_handle );
}


void ICACHE_FLASH_ATTR http_callback_example( char * response, int http_status, char * full_response )
{
	os_printf( "http_status=%d\n", http_status );
	if ( http_status != HTTP_STATUS_GENERIC_ERROR )
	{
		os_printf( "strlen(full_response)=%d\n", strlen( full_response ) );
		os_printf( "response=%s<EOF>\n", response );
	}
}
