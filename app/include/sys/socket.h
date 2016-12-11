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

#ifndef ESP_SOCKET_H_
#define ESP_SOCKET_H_

#include "lwip/inet.h"
#include "lwip/opt.h"
#include "sys/ringbuf.h"

#if 0
#define ESP_LOG			os_printf
#else
#define ESP_LOG(...)
#endif

#define NUM_SOCKETS 3

#define ESP_OK          0    /* No error, everything OK. */
#define ESP_MEM        -1    /* Out of memory error.     */
#define ESP_TIMEOUT    -3    /* Timeout.                 */
#define ESP_RTE        -4    /* Routing problem.         */
#define ESP_INPROGRESS  -5   /* Operation in progress    */
#define ESP_MAXNUM		-7	 /* Total number exceeds the set maximum*/

#define ESP_ABRT       -8    /* Connection aborted.      */
#define ESP_RST        -9    /* Connection reset.        */
#define ESP_CLSD       -10   /* Connection closed.       */
#define ESP_CONN       -11   /* Not connected.           */

#define ESP_ARG        -12   /* Illegal argument.        */
#define ESP_IF		   -14	 /* Low_level error			 */
#define ESP_ISCONN     -15   /* Already connected.       */

typedef enum{
	NETCONN_STATE_NONE = 0,
	NETCONN_STATE_ESTABLISHED,
	NETCONN_STATE_WRITE,
	NETCONN_STATE_READ,
	NETCONN_STATE_CLOSED,
	NETCONN_STATE_ERROR,
	NETCONN_STATE_SUMNUM
}netconn_state;

#if (!defined(lwIP_unlikely))
#define lwIP_unlikely(Expression)	!!(Expression)
#endif

#define lwIP_ASSERT(Expression) do{if (!(Expression)) ESP_LOG("%d\n", __LINE__);}while(0)

#define lwIP_REQUIRE_ACTION(Expression,Label,Action) \
	do{\
		if (lwIP_unlikely(!(Expression))) \
		{\
			ESP_LOG("%d\n", __LINE__);\
			{Action;}\
			goto Label;\
		}\
	}while(0)

#define lwIP_REQUIRE_NOERROR(Expression,Label) \
	do{\
		int LocalError;\
		LocalError = (int)Expression;\
		if (lwIP_unlikely(LocalError != 0)) \
		{\
			ESP_LOG("%d 0x%x\n", __LINE__, LocalError);\
			goto Label;\
		}\
	}while(0)

#define lwIP_REQUIRE_NOERROR_ACTION(Expression,Label,Action) \
	do{\
		int LocalError;\
		LocalError = (int)Expression;\
		if (lwIP_unlikely(LocalError != 0)) \
		{\
			ESP_LOG("%d\n", __LINE__);\
			{Action;}\
			goto Label;\
		}\
	}while(0)

#define lwIP_EVENT_PARSE(s, error)                          \
  do {                                                         \
	  mbedtls_parse_internal(s, error);                              \
  } while (0)

#define lwIP_EVENT_THREAD(s, event, error)	\
  do {	\
		mbedtls_parse_thread(s, event, error); \
  }while(0)
  
typedef enum{
	ENTCONN_EVENT_NONE = 0,
	NETCONN_EVENT_ESTABLISHED = 1,
	ENTCONN_EVENT_RECV = 2,
	NETCONN_EVENT_SEND = 3,
	NETCONN_EVENT_ERROR = 4,
	NETCONN_EVENT_CLOSE = 5,
	NETCONN_EVENT_SUMNUM = 6
}netconn_event;

typedef enum _netconn_type {
	NETCONN_INVALID = 0,
	/* ESPCONN_TCP Group */
	NETCONN_TCP = 0x10,
	/* ESPCONN_UDP Group */
	NETCONN_UDP = 0x20,
} netconn_type;

/* members are in network byte order */
struct sockaddr_in {
	u8_t sin_len;
	u8_t sin_family;
	u16_t sin_port;
	struct in_addr sin_addr;
	char sin_zero[8];
};

/** A netconn descriptor */
typedef struct _lwIP_netconn{
	/** flags blocking or nonblocking defines */
	uint8 flags;
	/** type of the lwIP_netconn (TCP, UDP) */
	netconn_type type;
	/** current state of the lwIP_netconn */
	netconn_state state;
	/** the lwIP internal protocol control block */
	struct tcp_pcb *tcp;

	/**the new socket is unknown and sync*/
	void *acceptmbox;

	/**the lwIP internal buffer control block*/
	ringbuf *readbuf;
	/** only used for socket layer */
	int socket;
}lwIP_netconn, *lwIPNetconn;

/** Contains all internal pointers and states used for a socket */
typedef struct _lwIP_sock{
	/** sockets currently are built on lwIP_netconn, each socket has one lwIP_netconn */
	lwIP_netconn *conn;
	/** data that was left from the previous read */
	u32_t recv_index;
	u32_t send_index;
	u32_t recv_data_len;
	void  *send_buffer;
}lwIP_sock;

typedef uint8 sa_family_t;

struct sockaddr {
	uint8 sa_len;
	sa_family_t sa_family;
	char sa_data[14];
};

typedef uint32 socklen_t;

#define NETCONNTYPE_GROUP(t)         ((t)&0xF0)
#define NETCONNTYPE_DATAGRAM(t)      ((t)&0xE0)

#define AF_UNSPEC       0
#define AF_INET         2

/* Socket protocol types (TCP/UDP/RAW) */
#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

#define lwIPThreadPrio 25
#define lwIPThreadQueueLen 15

/*
 * Option flags per-socket. These must match the SOF_ flags in ip.h (checked in init.c)
 */
#define  SO_DEBUG       0x0001 /* Unimplemented: turn on debugging info recording */
#define  SO_ACCEPTCONN  0x0002 /* socket has had listen() */
#define  SO_REUSEADDR   0x0004 /* Allow local address reuse */
#define  SO_KEEPALIVE   0x0008 /* keep connections alive */
#define  SO_DONTROUTE   0x0010 /* Unimplemented: just use interface addresses */
#define  SO_BROADCAST   0x0020 /* permit to send and to receive broadcast messages (see IP_SOF_BROADCAST option) */
#define  SO_USELOOPBACK 0x0040 /* Unimplemented: bypass hardware when possible */
#define  SO_LINGER      0x0080 /* linger on close if data present */
#define  SO_OOBINLINE   0x0100 /* Unimplemented: leave received OOB data in line */
#define  SO_REUSEPORT   0x0200 /* Unimplemented: allow local address & port reuse */

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF    0x1001    /* Unimplemented: send buffer size */
#define SO_RCVBUF    0x1002    /* receive buffer size */
#define SO_TYPE      0x1008    /* get socket type */

#define IPPROTO_IP      0
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

#define  SOL_SOCKET  0xfff    /* options for socket level */

#if LWIP_TCP
/*
 * Options for level IPPROTO_TCP
 */
#define TCP_NODELAY    0x01    /* don't delay send to coalesce packets */
#define TCP_KEEPALIVE  0x02    /* send KEEPALIVE probes when idle for pcb->keep_idle milliseconds */
#define TCP_KEEPIDLE   0x03    /* set pcb->keep_idle  - Same as TCP_KEEPALIVE, but use seconds for get/setsockopt */
#define TCP_KEEPINTVL  0x04    /* set pcb->keep_intvl - Use seconds for get/setsockopt */
#define TCP_KEEPCNT    0x05    /* set pcb->keep_cnt   - Use number of probes sent for get/setsockopt */
#endif /* LWIP_TCP */

/* commands for fnctl */
#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif

/* File status flags and file access modes for fnctl,
 these are bits in an int. */
#ifndef O_NONBLOCK
#define O_NONBLOCK  1 /* nonblocking I/O */
#endif
#ifndef O_NDELAY
#define O_NDELAY    1 /* same as O_NONBLOCK, for compatibility */
#endif

struct timeval {
	long tv_sec; /* seconds */
	long tv_usec; /* and microseconds */
};
/* Flags for struct netconn.flags (u8_t) */
/** TCP: when data passed to netconn_write doesn't fit into the send buffer,
 this temporarily stores whether to wake up the original application task
 if data couldn't be sent in the first try. */
#define NETCONN_FLAG_WRITE_DELAYED            0x01
/** Should this netconn avoid blocking? */
#define NETCONN_FLAG_NON_BLOCKING             0x02
/** Was the last connect action a non-blocking one? */
#define NETCONN_FLAG_IN_NONBLOCKING_CONNECT   0x04
/** If this is set, a TCP netconn must call netconn_recved() to update
 the TCP receive window (done automatically if not set). */
#define NETCONN_FLAG_NO_AUTO_RECVED           0x08
/** If a nonblocking write has been rejected before, poll_tcp needs to
 check if the netconn is writable again */
#define NETCONN_FLAG_CHECK_WRITESPACE         0x10

/** Set the blocking status of netconn calls (@todo: write/send is missing) */
#define netconn_set_nonblocking(conn, val)  do { if(val) { \
  (conn)->flags |= NETCONN_FLAG_NON_BLOCKING; \
} else { \
  (conn)->flags &= ~ NETCONN_FLAG_NON_BLOCKING; }} while(0)
/** Get the blocking status of netconn calls (@todo: write/send is missing) */
#define netconn_is_nonblocking(conn)        (((conn)->flags & NETCONN_FLAG_NON_BLOCKING) != 0)

/* FD_SET used for lwip_select */
#ifndef FD_SET
#undef  FD_SETSIZE
/* Make FD_SETSIZE match NUM_SOCKETS in socket.c */
#define FD_SETSIZE    NUM_SOCKETS
#define FD_SET(n, p)  ((p)->fd_bits[(n)/8] |=  (1 << ((n) & 7)))
#define FD_CLR(n, p)  ((p)->fd_bits[(n)/8] &= ~(1 << ((n) & 7)))
#define FD_ISSET(n,p) ((p)->fd_bits[(n)/8] &   (1 << ((n) & 7)))
#define FD_ZERO(p)    os_memset((void*)(p),0,sizeof(*(p)))

typedef struct fd_set {
	unsigned char fd_bits[(FD_SETSIZE + 7) / 8];
} fd_set;

#endif /* FD_SET */

void lwip_socket_init(void);

int lwip_accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int lwip_bind(int s, const struct sockaddr *name, socklen_t namelen);
int lwip_shutdown(int s, int how);
int lwip_getpeername(int s, struct sockaddr *name, socklen_t *namelen);
int lwip_getsockname(int s, struct sockaddr *name, socklen_t *namelen);
int lwip_getsockopt(int s, int level, int optname, void *optval,socklen_t *optlen);
int lwip_setsockopt(int s, int level, int optname, const void *optval,socklen_t optlen);
int lwip_close(int s);
int lwip_connect(int s, const struct sockaddr *name, socklen_t namelen);
int lwip_listen(int s, int backlog);
int lwip_recv(int s, void *mem, size_t len, int flags);
int lwip_read(int s, void *mem, size_t len);
int lwip_recvfrom(int s, void *mem, size_t len, int flags,struct sockaddr *from, socklen_t *fromlen);
int lwip_send(int s, const void *dataptr, size_t size, int flags);
int lwip_sendto(int s, const void *dataptr, size_t size, int flags,const struct sockaddr *to, socklen_t tolen);
int lwip_socket(int domain, int type, int protocol);
int lwip_write(int s, const void *dataptr, size_t size);
int lwip_select(int maxfdp1, fd_set *readset, fd_set *writeset,fd_set *exceptset, struct timeval *timeout);
int lwip_ioctl(int s, long cmd, void *argp);
int lwip_fcntl(int s, int cmd, int val);
uint32_t lwip_getul(char *str);

#define accept(a,b,c)         lwip_accept(a,b,c)
#define bind(a,b,c)           lwip_bind(a,b,c)
#define shutdown(a,b)         lwip_shutdown(a,b)
#define closesocket(s)        lwip_close(s)
#define connect(a,b,c)        lwip_connect(a,b,c)
#define getsockname(a,b,c)    lwip_getsockname(a,b,c)
#define getpeername(a,b,c)    lwip_getpeername(a,b,c)
#define setsockopt(a,b,c,d,e) lwip_setsockopt(a,b,c,d,e)
#define getsockopt(a,b,c,d,e) lwip_getsockopt(a,b,c,d,e)
#define listen(a,b)           lwip_listen(a,b)
#define recv(a,b,c,d)         lwip_recv(a,b,c,d)
#define recvfrom(a,b,c,d,e,f) lwip_recvfrom(a,b,c,d,e,f)
#define send(a,b,c,d)         lwip_send(a,b,c,d)
#define sendto(a,b,c,d,e,f)   lwip_sendto(a,b,c,d,e,f)
#define socket(a,b,c)         lwip_socket(a,b,c)
#define select(a,b,c,d,e)     lwip_select(a,b,c,d,e)
#define ioctlsocket(a,b,c)    lwip_ioctl(a,b,c)

#define read(a,b,c)           lwip_read(a,b,c)
#define write(a,b,c)          lwip_write(a,b,c)
#define close(s)              lwip_close(s)
#define getul(s)			  lwip_getul(s)

#endif /* ESPCONN_SOCKT_H_ */
