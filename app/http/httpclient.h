/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Martin d'Allens <martin.dallens@gmail.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 */

#ifndef __HTTPCLIENT_H__
#define __HTTPCLIENT_H__

#if defined(GLOBAL_DEBUG_ON)
#define HTTPCLIENT_DEBUG_ON
#endif
#if defined(HTTPCLIENT_DEBUG_ON)
#define HTTPCLIENT_DEBUG(format, ...) os_printf(format, ##__VA_ARGS__)
#else
#define HTTPCLIENT_DEBUG(format, ...)
#endif

#if defined(USES_SDK_BEFORE_V140)
#define espconn_send espconn_sent
#define espconn_secure_send espconn_secure_sent
#endif

#ifndef HTTP_DEFAULT_USER_AGENT
#define HTTP_DEFAULT_USER_AGENT "ESP8266"
#endif

/*
 * In case of TCP or DNS error the callback is called with this status.
 */
#define HTTP_STATUS_GENERIC_ERROR  (-1)

/*
 * Size of http responses that will cause an error.
 */
#define BUFFER_SIZE_MAX            (0x2000)

/*
 * Timeout of http request.
 */
#define HTTP_REQUEST_TIMEOUT_MS    (10000)

/*
 * "full_response" is a string containing all response headers and the response body.
 * "response_body and "http_status" are extracted from "full_response" for convenience.
 *
 * A successful request corresponds to an HTTP status code of 200 (OK).
 * More info at http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
 */
typedef void (* http_callback_t)(char * response_body, int http_status, char * full_response, int content_length);

/*
 * Call this function to skip URL parsing if the arguments are already in separate variables.
 */
void ICACHE_FLASH_ATTR http_raw_request(const char * hostname, int port, bool secure, const char * method, const char * path, const char * headers, const char * post_data, http_callback_t callback_handle);

/*
 * Request data from URL use custom method.
 * The data should be encoded as any format.
 * Try:
 * http_request("http://httpbin.org/post", "OPTIONS", "Content-type: text/plain", "Hello world", http_callback_example);
 */
void ICACHE_FLASH_ATTR http_request(const char * url, const char * method, const char * headers, const char * post_data, http_callback_t callback_handle);


#endif // __HTTPCLIENT_H__
