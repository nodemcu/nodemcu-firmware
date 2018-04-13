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

#include "osapi.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "limits.h"
#include "stdlib.h"

#include "c_types.h"
#include "c_string.h"
#include "c_stdlib.h"
#include "c_stdio.h"

#include "websocketclient.h"

// Depends on 'crypto' module for sha1
#include "../crypto/digests.h"
#include "../crypto/mech.h"

#include "pm/swtimer.h"

#define PROTOCOL_SECURE "wss://"
#define PROTOCOL_INSECURE "ws://"

#define PORT_SECURE 443
#define PORT_INSECURE 80
#define PORT_MAX_VALUE 65535

#define WS_INIT_REQUEST  "GET %s HTTP/1.1\r\n"\
                         "Host: %s:%d\r\n"

#define WS_INIT_REQUEST_LENGTH 30
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_GUID_LENGTH 36

#define WS_HTTP_SWITCH_PROTOCOL_HEADER "HTTP/1.1 101"
#define WS_HTTP_SEC_WEBSOCKET_ACCEPT "Sec-WebSocket-Accept:"

#define WS_CONNECT_TIMEOUT_MS 10 * 1000
#define WS_PING_INTERVAL_MS 30 * 1000
#define WS_FORCE_CLOSE_TIMEOUT_MS 5 * 1000
#define WS_UNHEALTHY_THRESHOLD 2

#define WS_OPCODE_CONTINUATION 0x0
#define WS_OPCODE_TEXT 0x1
#define WS_OPCODE_BINARY 0x2
#define WS_OPCODE_CLOSE 0x8
#define WS_OPCODE_PING 0x9
#define WS_OPCODE_PONG 0xA

static const header_t DEFAULT_HEADERS[] = {
  {"User-Agent", "ESP8266"},
  {"Sec-WebSocket-Protocol", "chat"},
  {0}
};
static const header_t *EMPTY_HEADERS = DEFAULT_HEADERS + sizeof(DEFAULT_HEADERS) / sizeof(header_t) - 1;

static char *cryptoSha1(char *data, unsigned int len) {
  SHA1_CTX ctx;
  SHA1Init(&ctx);
  SHA1Update(&ctx, data, len);
  
  uint8_t *digest = (uint8_t *) c_zalloc(20);
  SHA1Final(digest, &ctx);
  return (char *) digest; // Requires free
}

static const char *bytes64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *base64Encode(char *data, unsigned int len) {
  int blen = (len + 2) / 3 * 4;

  char *out = (char *) c_zalloc(blen + 1);
  out[blen] = '\0';
  int j = 0, i;
  for (i = 0; i < len; i += 3) {
    int a = data[i];
    int b = (i + 1 < len) ? data[i + 1] : 0;
    int c = (i + 2 < len) ? data[i + 2] : 0;
    out[j++] = bytes64[a >> 2];
    out[j++] = bytes64[((a & 3) << 4) | (b >> 4)];
    out[j++] = (i + 1 < len) ? bytes64[((b & 15) << 2) | (c >> 6)] : 61;
    out[j++] = (i + 2 < len) ? bytes64[(c & 63)] : 61;
  }

  return out; // Requires free
}

static void generateSecKeys(char **key, char **expectedKey) {
  char rndData[16];
  int i;
  for (i = 0; i < 16; i++) {
    rndData[i] = (char) os_random();
  }

  *key = base64Encode(rndData, 16);

  // expectedKey = b64(sha1(keyB64 + GUID))
  char keyWithGuid[24 + WS_GUID_LENGTH];
  memcpy(keyWithGuid, *key, 24);
  memcpy(keyWithGuid + 24, WS_GUID, WS_GUID_LENGTH);

  char *keyEncrypted = cryptoSha1(keyWithGuid, 24 + WS_GUID_LENGTH);
  *expectedKey = base64Encode(keyEncrypted, 20);

  os_free(keyEncrypted);
}

static char *_strcpy(char *dst, char *src) {
    while(*dst++ = *src++);
    return dst - 1;
}

static int headers_length(const header_t *headers) {
  int length = 0;
  for(; headers->key; headers++)
    length += strlen(headers->key) + strlen(headers->value) + 4;
  return length;
}

static char *sprintf_headers(char *buf, ...) {
  char *dst = buf;
  va_list args;
  va_start(args, buf);
  for(header_t *header_set = va_arg(args, header_t *); header_set; header_set = va_arg(args, header_t *))
    for(header_t *header = header_set; header->key; header++) {
      va_list args2;
      va_start(args2, buf);
      for(header_t *header_set2 = va_arg(args2, header_t *); header_set2; header_set2 = va_arg(args2, header_t *))
        for(header_t *header2 = header_set2; header2->key; header2++) {
          if(header == header2)
            goto ok;
          if(!strcasecmp(header->key, header2->key))
            goto skip;
        }
ok:
      dst = _strcpy(dst, header->key);
      dst = _strcpy(dst, ": ");
      dst = _strcpy(dst, header->value);
      dst = _strcpy(dst, "\r\n");
skip:;
    }
  dst = _strcpy(dst, "\r\n");
  return dst;
}

static void ws_closeSentCallback(void *arg) {
  NODE_DBG("ws_closeSentCallback \n");
  struct espconn *conn = (struct espconn *) arg;
  ws_info *ws = (ws_info *) conn->reverse;

  if (ws == NULL) {
    NODE_DBG("ws is unexpectly null\n");
    return;
  }

  ws->knownFailureCode = -6;

  if (ws->isSecure)
    espconn_secure_disconnect(conn);
  else
    espconn_disconnect(conn);
}

static void ws_sendFrame(struct espconn *conn, int opCode, const char *data, unsigned short len) {
  NODE_DBG("ws_sendFrame %d %d\n", opCode, len);
  ws_info *ws = (ws_info *) conn->reverse;
  
  if (ws->connectionState == 4) {
    NODE_DBG("already in closing state\n");
    return;
  } else if (ws->connectionState != 3) {
    NODE_DBG("can't send message while not in a connected state\n");
    return;
  }

  char *b = c_zalloc(10 + len); // 10 bytes = worst case scenario for framming
  if (b == NULL) {
    NODE_DBG("Out of memory when receiving message, disconnecting...\n");

    ws->knownFailureCode = -16;
    if (ws->isSecure)
      espconn_secure_disconnect(conn);
    else
      espconn_disconnect(conn);
    return;
  }

  b[0] = 1 << 7; // has fin
  b[0] += opCode;
  b[1] = 1 << 7; // has mask
  int bufOffset;
  if (len < 126) {
    b[1] += len;
    bufOffset = 2;
  } else if (len < 0x10000) {
    b[1] += 126;
    b[2] = len >> 8;
    b[3] = len;
    bufOffset = 4;
  } else {
    b[1] += 127;
    b[2] = len >> 24;
    b[3] = len >> 16;
    b[4] = len >> 8;
    b[5] = len;
    bufOffset = 6;
  }

  // Random mask:
  b[bufOffset] = (char) os_random();
  b[bufOffset + 1] = (char) os_random();
  b[bufOffset + 2] = (char) os_random();
  b[bufOffset + 3] = (char) os_random();
  bufOffset += 4;

  // Copy data to buffer
  memcpy(b + bufOffset, data, len);

  // Apply mask to encode payload
  int i;
  for (i = 0; i < len; i++) {
    b[bufOffset + i] ^= b[bufOffset - 4 + i % 4]; 
  }
  bufOffset += len;

  NODE_DBG("b[0] = %d \n", b[0]);
  NODE_DBG("b[1] = %d \n", b[1]);
  NODE_DBG("b[2] = %d \n", b[2]);
  NODE_DBG("b[3] = %d \n", b[3]);
  NODE_DBG("b[4] = %d \n", b[4]);
  NODE_DBG("b[5] = %d \n", b[5]);
  NODE_DBG("b[6] = %d \n", b[6]);
  NODE_DBG("b[7] = %d \n", b[7]);
  NODE_DBG("b[8] = %d \n", b[8]);
  NODE_DBG("b[9] = %d \n", b[9]);

  NODE_DBG("sending message\n");
  if (ws->isSecure)
    espconn_secure_send(conn, (uint8_t *) b, bufOffset);
  else
    espconn_send(conn, (uint8_t *) b, bufOffset);

  os_free(b);
}

static void ws_sendPingTimeout(void *arg) {
  NODE_DBG("ws_sendPingTimeout \n");
  struct espconn *conn = (struct espconn *) arg;
  ws_info *ws = (ws_info *) conn->reverse;

  if (ws->unhealthyPoints == WS_UNHEALTHY_THRESHOLD) {
    // several pings were sent but no pongs nor messages
    ws->knownFailureCode = -19;

    if (ws->isSecure)
      espconn_secure_disconnect(conn);
    else
      espconn_disconnect(conn);
    return;
  }

  ws_sendFrame(conn, WS_OPCODE_PING, NULL, 0);
  ws->unhealthyPoints += 1;
}

static void ws_receiveCallback(void *arg, char *buf, unsigned short len) {
  NODE_DBG("ws_receiveCallback %d \n", len);
  struct espconn *conn = (struct espconn *) arg;
  ws_info *ws = (ws_info *) conn->reverse;

  ws->unhealthyPoints = 0; // received data, connection is healthy
  os_timer_disarm(&ws->timeoutTimer); // reset ping check
  os_timer_arm(&ws->timeoutTimer, WS_PING_INTERVAL_MS, true);


  char *b = buf;
  if (ws->frameBuffer != NULL) { // Append previous frameBuffer with new content
    NODE_DBG("Appending new frameBuffer to old one \n");

    ws->frameBuffer = c_realloc(ws->frameBuffer, ws->frameBufferLen + len);
    if (ws->frameBuffer == NULL) {
      NODE_DBG("Failed to allocate new framebuffer, disconnecting...\n");

      ws->knownFailureCode = -8;
      if (ws->isSecure)
        espconn_secure_disconnect(conn);
      else
        espconn_disconnect(conn);
      return;
    }
    memcpy(ws->frameBuffer + ws->frameBufferLen, b, len);

    ws->frameBufferLen += len;

    len = ws->frameBufferLen;
    b = ws->frameBuffer;
    NODE_DBG("New frameBufferLen: %d\n", len);
  }

  while (b != NULL) { // several frames can be present, b pointer will be moved to the next frame
    NODE_DBG("b[0] = %d \n", b[0]);
    NODE_DBG("b[1] = %d \n", b[1]);
    NODE_DBG("b[2] = %d \n", b[2]);
    NODE_DBG("b[3] = %d \n", b[3]);
    NODE_DBG("b[4] = %d \n", b[4]);
    NODE_DBG("b[5] = %d \n", b[5]);
    NODE_DBG("b[6] = %d \n", b[6]);
    NODE_DBG("b[7] = %d \n", b[7]);

    int isFin = b[0] & 0x80 ? 1 : 0;
    int opCode = b[0] & 0x0f;
    int hasMask = b[1] & 0x80 ? 1 : 0;
    uint64_t payloadLength = b[1] & 0x7f;
    int bufOffset = 2;
    if (payloadLength == 126) {
      payloadLength = (b[2] << 8) + b[3];
      bufOffset = 4;
    } else if (payloadLength == 127) { // this will clearly not hold in heap, abort??
      payloadLength = (b[2] << 24) + (b[3] << 16) + (b[4] << 8) + b[5];
      bufOffset = 6;
    }

    if (hasMask) {
      int maskOffset = bufOffset;
      bufOffset += 4;

      int i;
      for (i = 0; i < payloadLength; i++) {
        b[bufOffset + i] ^= b[maskOffset + i % 4]; // apply mask to decode payload
      }
    }

    if (payloadLength > len - bufOffset) {
      NODE_DBG("INCOMPLETE Frame \n");
      if (ws->frameBuffer == NULL) {
        NODE_DBG("Allocing new frameBuffer \n");
        ws->frameBuffer = c_zalloc(len);
        if (ws->frameBuffer == NULL) {
          NODE_DBG("Failed to allocate framebuffer, disconnecting... \n");

          ws->knownFailureCode = -9;
          if (ws->isSecure)
            espconn_secure_disconnect(conn);
          else
            espconn_disconnect(conn);
          return;
        }
        memcpy(ws->frameBuffer, b, len);
        ws->frameBufferLen = len;
      }
      break; // since the buffer were already concat'ed, wait for the next receive
    }

    if (!isFin) {
      NODE_DBG("PARTIAL frame! Should concat payload and later restore opcode\n");
      if(ws->payloadBuffer == NULL) {
        NODE_DBG("Allocing new payloadBuffer \n");
        ws->payloadBuffer = c_zalloc(payloadLength);
        if (ws->payloadBuffer == NULL) {
          NODE_DBG("Failed to allocate payloadBuffer, disconnecting...\n");

          ws->knownFailureCode = -10;
          if (ws->isSecure)
            espconn_secure_disconnect(conn);
          else
            espconn_disconnect(conn);
          return;
        }
        memcpy(ws->payloadBuffer, b + bufOffset, payloadLength);
        ws->frameBufferLen = payloadLength;
        ws->payloadOriginalOpCode = opCode;
      } else {
        NODE_DBG("Appending new payloadBuffer to old one \n");
        ws->payloadBuffer = c_realloc(ws->payloadBuffer, ws->payloadBufferLen + payloadLength);
        if (ws->payloadBuffer == NULL) {
          NODE_DBG("Failed to allocate new framebuffer, disconnecting...\n");

          ws->knownFailureCode = -11;
          if (ws->isSecure)
            espconn_secure_disconnect(conn);
          else
            espconn_disconnect(conn);
          return;
        }
        memcpy(ws->payloadBuffer + ws->payloadBufferLen, b + bufOffset, payloadLength);

        ws->payloadBufferLen += payloadLength;
      }
    } else {
      char *payload;
      if (opCode == WS_OPCODE_CONTINUATION) {
        NODE_DBG("restoring original opcode\n");
        if (ws->payloadBuffer == NULL) {
          NODE_DBG("Got FIN continuation frame but didn't receive any beforehand, disconnecting...\n");

          ws->knownFailureCode = -15;
          if (ws->isSecure)
            espconn_secure_disconnect(conn);
          else
            espconn_disconnect(conn);
          return;
        }
        // concat buffer with payload
        payload = c_zalloc(ws->payloadBufferLen + payloadLength);

        if (payload == NULL) {
          NODE_DBG("Failed to allocate new framebuffer, disconnecting...\n");

          ws->knownFailureCode = -12;
          if (ws->isSecure)
            espconn_secure_disconnect(conn);
          else
            espconn_disconnect(conn);
          return;
        }
        memcpy(payload, ws->payloadBuffer, ws->payloadBufferLen);
        memcpy(payload + ws->payloadBufferLen, b + bufOffset, payloadLength);

        os_free(ws->payloadBuffer); // free previous buffer
        ws->payloadBuffer = NULL;

        payloadLength += ws->payloadBufferLen;
        ws->payloadBufferLen = 0;

        opCode = ws->payloadOriginalOpCode;
        ws->payloadOriginalOpCode = 0;
      } else {
        int extensionDataOffset = 0;

        if (opCode == WS_OPCODE_CLOSE && payloadLength > 0) {
          unsigned int reasonCode = b[bufOffset] << 8 + b[bufOffset + 1];
          NODE_DBG("Closing due to: %d\n", reasonCode); // Must not be shown to client as per spec
          extensionDataOffset += 2;
        }

        payload = c_zalloc(payloadLength - extensionDataOffset + 1);
        if (payload == NULL) {
          NODE_DBG("Failed to allocate payload, disconnecting...\n");

          ws->knownFailureCode = -13;
          if (ws->isSecure)
            espconn_secure_disconnect(conn);
          else
            espconn_disconnect(conn);
          return;
        }

        memcpy(payload, b + bufOffset + extensionDataOffset, payloadLength - extensionDataOffset);
        payload[payloadLength - extensionDataOffset] = '\0';
      }

      NODE_DBG("isFin %d \n", isFin);
      NODE_DBG("opCode %d \n", opCode);
      NODE_DBG("hasMask %d \n", hasMask);
      NODE_DBG("payloadLength %d \n", payloadLength);
      NODE_DBG("len %d \n", len);
      NODE_DBG("bufOffset %d \n", bufOffset);

      if (opCode == WS_OPCODE_CLOSE) {
        NODE_DBG("Closing message: %s\n", payload); // Must not be shown to client as per spec

        espconn_regist_sentcb(conn, ws_closeSentCallback);
        ws_sendFrame(conn, WS_OPCODE_CLOSE, (const char *) (b + bufOffset), (unsigned short) payloadLength);
        ws->connectionState = 4;
      } else if (opCode == WS_OPCODE_PING) {
        ws_sendFrame(conn, WS_OPCODE_PONG, (const char *) (b + bufOffset), (unsigned short) payloadLength);
      } else if (opCode == WS_OPCODE_PONG) {
        // ping alarm was already reset...
      } else {
        if (ws->onReceive) ws->onReceive(ws, payloadLength, payload, opCode);
      }
      os_free(payload);
    }

    bufOffset += payloadLength;
    NODE_DBG("bufOffset %d \n", bufOffset);
    if (bufOffset == len) { // (bufOffset > len) won't happen here because it's being checked earlier
      b = NULL;
      if (ws->frameBuffer != NULL) { // the last frame inside buffer was processed
        os_free(ws->frameBuffer);
        ws->frameBuffer = NULL;
        ws->frameBufferLen = 0;
      }
    } else {
      len -= bufOffset;
      b += bufOffset; // move b to next frame
      if (ws->frameBuffer != NULL) {
        NODE_DBG("Reallocing frameBuffer to remove consumed frame\n");

        ws->frameBuffer = c_realloc(ws->frameBuffer, ws->frameBufferLen + len);
        if (ws->frameBuffer == NULL) {
          NODE_DBG("Failed to allocate new frame buffer, disconnecting...\n");

          ws->knownFailureCode = -14;
          if (ws->isSecure)
            espconn_secure_disconnect(conn);
          else
            espconn_disconnect(conn);
          return;
        }
        memcpy(ws->frameBuffer + ws->frameBufferLen, b, len);

        ws->frameBufferLen += len;
        b = ws->frameBuffer;
      }
    }
  }
}

static void ws_initReceiveCallback(void *arg, char *buf, unsigned short len) {
  NODE_DBG("ws_initReceiveCallback %d \n", len);
  struct espconn *conn = (struct espconn *) arg;
  ws_info *ws = (ws_info *) conn->reverse;

  // Check server is switch protocols
  if (strstr(buf, WS_HTTP_SWITCH_PROTOCOL_HEADER) == NULL) {
      NODE_DBG("Server is not switching protocols\n");
      ws->knownFailureCode = -17;
    if (ws->isSecure)
      espconn_secure_disconnect(conn);
    else
      espconn_disconnect(conn);
    return;
  }

  // Check server has valid sec key
  if (strstr(buf, ws->expectedSecKey) == NULL) {
    NODE_DBG("Server has invalid response\n");
    ws->knownFailureCode = -7;
    if (ws->isSecure)
      espconn_secure_disconnect(conn);
    else
      espconn_disconnect(conn);
    return;
  }

  NODE_DBG("Server response is valid, it's now a websocket!\n");

  os_timer_disarm(&ws->timeoutTimer);
  os_timer_setfn(&ws->timeoutTimer, (os_timer_func_t *) ws_sendPingTimeout, conn);
  SWTIMER_REG_CB(ws_sendPingTimeout, SWTIMER_RESUME)
  os_timer_arm(&ws->timeoutTimer, WS_PING_INTERVAL_MS, true);

  espconn_regist_recvcb(conn, ws_receiveCallback);

  if (ws->onConnection) ws->onConnection(ws);

  char *data = strstr(buf, "\r\n\r\n");
  unsigned short dataLength = len - (data - buf) - 4;

  NODE_DBG("dataLength = %d\n", len - (data - buf) - 4);

  if (data != NULL && dataLength > 0) { // handshake already contained a frame
    ws_receiveCallback(arg, data + 4, dataLength);
  }
}

static void connect_callback(void *arg) {
  NODE_DBG("Connected\n");
  struct espconn *conn = (struct espconn *) arg;
  ws_info *ws = (ws_info *) conn->reverse;
  ws->connectionState = 3;

  espconn_regist_recvcb(conn, ws_initReceiveCallback);

  char *key;
  generateSecKeys(&key, &ws->expectedSecKey);

  header_t headers[] = {
	  {"Upgrade", "websocket"},
	  {"Connection", "Upgrade"},
	  {"Sec-WebSocket-Key", key},
	  {"Sec-WebSocket-Version", "13"},
	  {0}
  };

  const header_t *extraHeaders = ws->extraHeaders ? ws->extraHeaders : EMPTY_HEADERS;

  char buf[WS_INIT_REQUEST_LENGTH + strlen(ws->path) + strlen(ws->hostname) +
	  headers_length(DEFAULT_HEADERS) + headers_length(headers) + headers_length(extraHeaders) + 2];

  int len = os_sprintf(
                  buf,
                  WS_INIT_REQUEST,
                  ws->path,
                  ws->hostname,
                  ws->port
		  );

  len = sprintf_headers(buf + len, headers, extraHeaders, DEFAULT_HEADERS, 0) - buf;

  os_free(key);
  NODE_DBG("request: %s", buf);
  if (ws->isSecure)
    espconn_secure_send(conn, (uint8_t *) buf, len);
  else
    espconn_send(conn, (uint8_t *) buf, len);
}

static void disconnect_callback(void *arg) {
  NODE_DBG("disconnect_callback\n");
  struct espconn *conn = (struct espconn *) arg;
  ws_info *ws = (ws_info *) conn->reverse;

  ws->connectionState = 4;

  os_timer_disarm(&ws->timeoutTimer);

  NODE_DBG("ws->hostname %d\n", ws->hostname);
  os_free(ws->hostname);
  NODE_DBG("ws->path %d\n ", ws->path);
  os_free(ws->path);

  if (ws->expectedSecKey != NULL) {
    os_free(ws->expectedSecKey);
  }

  if (ws->frameBuffer != NULL) {
    os_free(ws->frameBuffer);
  }

  if (ws->payloadBuffer != NULL) {
    os_free(ws->payloadBuffer);
  }

  if (conn->proto.tcp != NULL) {
    os_free(conn->proto.tcp);
  }

  NODE_DBG("conn %d\n", conn);
  espconn_delete(conn);

  NODE_DBG("freeing conn1 \n");

  os_free(conn);
  ws->conn = NULL;

  if (ws->onFailure) {
    if (ws->knownFailureCode) ws->onFailure(ws, ws->knownFailureCode);
    else ws->onFailure(ws, -99);
  }
}

static void ws_connectTimeout(void *arg) {
  NODE_DBG("ws_connectTimeout\n");
  struct espconn *conn = (struct espconn *) arg;
  ws_info *ws = (ws_info *) conn->reverse;

  ws->knownFailureCode = -18;
  disconnect_callback(arg);
}

static void error_callback(void * arg, sint8 errType) {
  NODE_DBG("error_callback %d\n", errType);
  struct espconn *conn = (struct espconn *) arg;
  ws_info *ws = (ws_info *) conn->reverse;

  ws->knownFailureCode = ((int) errType) - 100;
  disconnect_callback(arg);
}

static void dns_callback(const char *hostname, ip_addr_t *addr, void *arg) {
  NODE_DBG("dns_callback\n");
  struct espconn *conn = (struct espconn *) arg;
  ws_info *ws = (ws_info *) conn->reverse;

  if (ws->conn == NULL || ws->connectionState == 4) {
          return;
  }

  if (addr == NULL)  {
    ws->knownFailureCode = -5;
    disconnect_callback(arg);
    return;
  }

  ws->connectionState = 2;

  os_memcpy(conn->proto.tcp->remote_ip, addr, 4);

  espconn_regist_connectcb(conn, connect_callback);
  espconn_regist_disconcb(conn, disconnect_callback);
  espconn_regist_reconcb(conn, error_callback);

  // Set connection timeout timer
  os_timer_disarm(&ws->timeoutTimer);
  os_timer_setfn(&ws->timeoutTimer, (os_timer_func_t *) ws_connectTimeout, conn);
  SWTIMER_REG_CB(ws_connectTimeout, SWTIMER_RESUME)
  os_timer_arm(&ws->timeoutTimer, WS_CONNECT_TIMEOUT_MS, false);

  if (ws->isSecure) {
    NODE_DBG("secure connecting \n");
    espconn_secure_connect(conn);
  }
  else {
    NODE_DBG("insecure connecting \n");
    espconn_connect(conn);
  }

  NODE_DBG("DNS found %s " IPSTR " \n", hostname, IP2STR(addr));
}

void ws_connect(ws_info *ws, const char *url) {
  NODE_DBG("ws_connect called\n");

  if (ws == NULL) {
    NODE_DBG("ws_connect ws_info argument is null!");
    return;
  }

  if (url == NULL) {
    NODE_DBG("url is null!");
    return;
  }

  // Extract protocol - either ws or wss
  bool isSecure = c_strncasecmp(url, PROTOCOL_SECURE, strlen(PROTOCOL_SECURE)) == 0;

  if (isSecure) {
    url += strlen(PROTOCOL_SECURE);
  } else {
    if (c_strncasecmp(url, PROTOCOL_INSECURE, strlen(PROTOCOL_INSECURE)) != 0) {
      NODE_DBG("Failed to extract protocol from: %s\n", url);
      if (ws->onFailure) ws->onFailure(ws, -1);
      return;
    }
    url += strlen(PROTOCOL_INSECURE);
  }

  // Extract path - it should start with '/'
  char *path = c_strchr(url, '/');

  // Extract hostname, possibly including port
  char hostname[256];
  if (path) {
    if (path - url >= sizeof(hostname)) {
      NODE_DBG("Hostname too large");
      if (ws->onFailure) ws->onFailure(ws, -2);
      return;
    }
    memcpy(hostname, url, path - url);
    hostname[path - url] = '\0';
  } else {
    // no path found, assuming the url only refers to the hostname and possibly the port
    memcpy(hostname, url, strlen(url));
    hostname[strlen(url)] = '\0';
    path = "/";
  }

  // Extract port from hostname, if available
  char *portInHostname = strchr(hostname, ':');
  int port;
  if (portInHostname) {
    port = atoi(portInHostname + 1);
    if (port <= 0 || port > PORT_MAX_VALUE) {
      NODE_DBG("Invalid port number\n");
      if (ws->onFailure) ws->onFailure(ws, -3);
      return;
    }
    hostname[strlen(hostname) - strlen(portInHostname)] = '\0'; // remove port from hostname
  } else {
    port = isSecure ? PORT_SECURE : PORT_INSECURE;
  }

  if (strlen(hostname) == 0) {
    NODE_DBG("Failed to extract hostname\n");
    if (ws->onFailure) ws->onFailure(ws, -4);
    return;
  }

  NODE_DBG("secure protocol = %d\n", isSecure);
  NODE_DBG("hostname = %s\n", hostname);
  NODE_DBG("port = %d\n", port);
  NODE_DBG("path = %s\n", path);

  // Prepare internal ws_info
  ws->connectionState = 1;
  ws->isSecure = isSecure;
  ws->hostname = c_strdup(hostname);
  ws->port = port;
  ws->path = c_strdup(path);
  ws->expectedSecKey = NULL;
  ws->knownFailureCode = 0;
  ws->frameBuffer = NULL;
  ws->frameBufferLen = 0;
  ws->payloadBuffer = NULL;
  ws->payloadBufferLen = 0;
  ws->payloadOriginalOpCode = 0;
  ws->unhealthyPoints = 0;

  // Prepare espconn
  struct espconn *conn = (struct espconn *) c_zalloc(sizeof(struct espconn));
  conn->type = ESPCONN_TCP;
  conn->state = ESPCONN_NONE;
  conn->proto.tcp = (esp_tcp *) c_zalloc(sizeof(esp_tcp));
  conn->proto.tcp->local_port = espconn_port();
  conn->proto.tcp->remote_port = ws->port;
  
  conn->reverse = ws;
  ws->conn = conn;

  // Attempt to resolve hostname address
  ip_addr_t  addr;
  err_t result = espconn_gethostbyname(conn, hostname, &addr, dns_callback);

  if (result == ESPCONN_INPROGRESS) {
    NODE_DBG("DNS pending\n");
  } else {
    dns_callback(hostname, &addr, conn);
  }

  return;
}

void ws_send(ws_info *ws, int opCode, const char *message, unsigned short length) {
  NODE_DBG("ws_send\n");
  ws_sendFrame(ws->conn, opCode, message, length);
}

static void ws_forceCloseTimeout(void *arg) {
  NODE_DBG("ws_forceCloseTimeout\n");
  struct espconn *conn = (struct espconn *) arg;
  ws_info *ws = (ws_info *) conn->reverse;

  if (ws->connectionState == 0 || ws->connectionState == 4) {
    return;
  }

  if (ws->isSecure)
    espconn_secure_disconnect(ws->conn);
  else
    espconn_disconnect(ws->conn);
}

void ws_close(ws_info *ws) {
  NODE_DBG("ws_close\n");

  if (ws->connectionState == 0 || ws->connectionState == 4) {
    return;
  }

  ws->knownFailureCode = 0; // no error as user requested to close
  if (ws->connectionState == 1) {
    disconnect_callback(ws->conn);
  } else {
    ws_sendFrame(ws->conn, WS_OPCODE_CLOSE, NULL, 0);

    os_timer_disarm(&ws->timeoutTimer);
    os_timer_setfn(&ws->timeoutTimer, (os_timer_func_t *) ws_forceCloseTimeout, ws->conn);
    SWTIMER_REG_CB(ws_forceCloseTimeout, SWTIMER_RESUME);
    os_timer_arm(&ws->timeoutTimer, WS_FORCE_CLOSE_TIMEOUT_MS, false);
  }
}
