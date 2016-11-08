/* Websocket client implementation
 *
 * Copyright (c) 2016 Luís Fonseca <miguelluisfonseca@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _WEBSOCKET_H_
#define _WEBSOCKET_H_

#include "osapi.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "limits.h"
#include "stdlib.h"

#if defined(USES_SDK_BEFORE_V140)
#define espconn_send espconn_sent
#define espconn_secure_send espconn_secure_sent
#endif

struct ws_info;

typedef void (*ws_onConnectionCallback)(struct ws_info *wsInfo);
typedef void (*ws_onReceiveCallback)(struct ws_info *wsInfo, int len, char *message, int opCode);
typedef void (*ws_onFailureCallback)(struct ws_info *wsInfo, int errorCode);

typedef struct {
	char *key;
	char *value;
} header_t;

typedef struct ws_info {
  int connectionState;

  bool isSecure;
  char *hostname;
  int port;
  char *path;
  char *expectedSecKey;
  header_t *extraHeaders;

  struct espconn *conn;
  void *reservedData;
  int knownFailureCode;

  char *frameBuffer;
  int frameBufferLen;

  char *payloadBuffer;
  int payloadBufferLen;
  int payloadOriginalOpCode;

  os_timer_t  timeoutTimer;
  int unhealthyPoints;

  ws_onConnectionCallback onConnection;
  ws_onReceiveCallback onReceive;
  ws_onFailureCallback onFailure;
} ws_info;

/*
 * Attempts to estabilish a websocket connection to the given url.
 */
void ws_connect(ws_info *wsInfo, const char *url);

/*
 * Sends a message with a given opcode.
 */
void ws_send(ws_info *wsInfo, int opCode, const char *message, unsigned short length);

/*
 * Disconnects existing conection and frees memory.
 */
void ws_close(ws_info *wsInfo);

#endif // _WEBSOCKET_H_
