
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

#include "lwip/netif.h"
#include "lwip/inet.h"
#include "netif/etharp.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"
#include "lwip/init.h"
#include "lwip/tcp_impl.h"
#include "lwip/memp.h"

#include "ets_sys.h"
#include "os_type.h"

#include "lwip/mem.h"
#include "sys/socket.h"

#ifdef MEMLEAK_DEBUG
static const char mem_debug_file[] ICACHE_RODATA_ATTR = __FILE__;
#endif

/** The global array of available sockets */
static lwIP_sock sockets[NUM_SOCKETS];

/** Table to quickly map an lwIP error (err_t) to a socket error
  * by using -err as an index */
static const int err_to_errno_table[] =
{
    0,             /* ERR_OK          0      No error, everything OK. */
//  ENOMEM,        /* ERR_MEM        -1      Out of memory error.     */
//  ENOBUFS,       /* ERR_BUF        -2      Buffer error.            */
//  EWOULDBLOCK,   /* ERR_TIMEOUT    -3      Timeout                  */
//  EHOSTUNREACH,  /* ERR_RTE        -4      Routing problem.         */
//  EINPROGRESS,   /* ERR_INPROGRESS -5      Operation in progress    */
//  EINVAL,        /* ERR_VAL        -6      Illegal value.           */
//  EWOULDBLOCK,   /* ERR_WOULDBLOCK -7      Operation would block.   */
//  ECONNABORTED,  /* ERR_ABRT       -8      Connection aborted.      */
//  ECONNRESET,    /* ERR_RST        -9      Connection reset.        */
//  ESHUTDOWN,     /* ERR_CLSD       -10     Connection closed.       */
//  ENOTCONN,      /* ERR_CONN       -11     Not connected.           */
//  EIO,           /* ERR_ARG        -12     Illegal argument.        */
//  EADDRINUSE,    /* ERR_USE        -13     Address in use.          */
    -1,            /* ERR_IF         -14     Low-level netif error    */
    -1,            /* ERR_ISCONN     -15     Already connected.       */
};

#define ERR_TO_ERRNO_TABLE_SIZE \
  (sizeof(err_to_errno_table)/sizeof(err_to_errno_table[0]))

#define err_to_errno(err) \
  ((unsigned)(-(err)) < ERR_TO_ERRNO_TABLE_SIZE ? \
    err_to_errno_table[-(err)] : EIO)

#ifdef ERRNO
#ifndef set_errno
#define set_errno(err) errno = (err)
#endif
#else /* ERRNO */
#define set_errno(err)
#endif /* ERRNO */

#define sock_set_errno(sk, e) do { \
  sk->err = (e); \
  set_errno(sk->err); \
} while (0)

static lwIP_sock *get_socket(int s);

static int find_socket(lwIP_netconn *newconn)
{
    int i = 0;
    lwIP_ASSERT(newconn);
    for (i = 0; i < NUM_SOCKETS; ++i)
    {
        if (sockets[i].conn && sockets[i].conn == newconn)
            return i;
    }
    return -1;
}

static void remove_tcp(lwIP_netconn *conn)
{
    struct tcp_pcb *pcb = NULL;
    sint8 ret = ERR_OK;
    lwIP_REQUIRE_ACTION(conn, exit, ret = ERR_ARG);
    lwIP_REQUIRE_ACTION(conn->tcp, exit, ret = ERR_ARG);
    pcb = conn->tcp;
    tcp_arg(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_poll(pcb, NULL, 0);
    tcp_err(pcb, NULL);
exit:
    return;
}

static void free_netconn(lwIP_netconn *netconn)
{
    lwIP_ASSERT(netconn);
    if (netconn->readbuf)
    {
        ringbuf_free(&netconn->readbuf);
    }
    
    os_free(netconn);
    netconn = NULL;
}

static lwIP_netconn *
netconn_alloc(netconn_type type, void *arg)
{
    sint8 ret = ERR_OK;
    lwIP_netconn *netconn = NULL;
    struct tcp_pcb *pcb = arg;
    lwIP_REQUIRE_ACTION(pcb, exit, ret = ERR_ARG);
    netconn = (lwIP_netconn *)os_zalloc(sizeof(lwIP_netconn));
    lwIP_REQUIRE_ACTION(netconn, exit, ret = ERR_MEM);
    netconn->readbuf = ringbuf_new(TCP_SND_BUF);
    lwIP_REQUIRE_ACTION(netconn->readbuf, exit, ret = ERR_MEM);
    system_overclock();
    netconn->socket = -1;
exit:
    if (ret != ERR_OK)
    {
        free_netconn(netconn);
    }
    return netconn;
}

static err_t netconn_getaddr(lwIP_netconn *conn, ip_addr_t *addr, u16_t *port, u8_t local)
{
    sint8 ret = ERR_OK;
    lwIP_REQUIRE_ACTION(conn, exit, ret = ERR_ARG);
    lwIP_REQUIRE_ACTION(addr, exit, ret = ERR_ARG);
    lwIP_REQUIRE_ACTION(port, exit, ret = ERR_ARG);
    lwIP_REQUIRE_ACTION(conn->tcp, exit, ret = ERR_ARG);
    *port = local==1 ? conn->tcp->local_port : conn->tcp->remote_port;
    *addr = local==1 ? conn->tcp->local_ip : conn->tcp->remote_ip;
exit:
    return ret;
}

/** Free a socket. The socket's netconn must have been
 * delete before!
 *
 * @param sock the socket to free
 * @param is_tcp != 0 for TCP sockets, used to free lastdata
 */
static void free_socket(lwIP_sock *sock)
{
    void *lastdata = NULL;
    sock->conn = NULL;
    sock->recv_data_len = 0;
    sock->recv_index = 0;
    sock->send_buffer = NULL;
    sock->send_index = 0;
}

static err_t recv_tcp(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    lwIP_netconn *newconn = arg;
	err = ESP_OK;
	lwIP_REQUIRE_ACTION(newconn, exit, err = ESP_ARG);
    
    if (p!= NULL)
    {
        struct pbuf *pthis = NULL;
        
        if (newconn->readbuf != NULL)
        {
            for (pthis = p; pthis != NULL; pthis = pthis->next)
            {
                newconn->state = NETCONN_STATE_READ;
                ringbuf_memcpy_into(newconn->readbuf, pthis->payload, pthis->len);
                tcp_recved(newconn->tcp, pthis->len);
				newconn->state = NETCONN_STATE_ESTABLISHED;
                lwIP_EVENT_PARSE(find_socket(newconn), ERR_OK);                
            }
			pbuf_free(p);
        }
        else
        {           
            tcp_recved(newconn->tcp, p->tot_len);
            pbuf_free(p);
            err = ERR_MEM;
        }        
    }
    else
    {
        lwIP_EVENT_PARSE(find_socket(newconn), NETCONN_EVENT_CLOSE);
    }
exit:
    return err;
}

static err_t poll_tcp(void *arg, struct tcp_pcb *pcb)
{
    lwIP_netconn *conn = arg;
    lwIP_ASSERT(conn);

    return ERR_OK;
}

static err_t sent_tcp(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    lwIP_netconn *conn = arg;
    lwIP_ASSERT(conn);
    conn->state = NETCONN_STATE_ESTABLISHED;                
	lwIP_EVENT_THREAD(find_socket(conn), NETCONN_EVENT_SEND, len);        
    return ERR_OK;
}

static void err_tcp(void *arg, err_t err)
{
    lwIP_netconn *conn = arg;       
    lwIP_ASSERT(conn);
    conn->state = NETCONN_STATE_ERROR;
    ESP_LOG("%s %d %p\n",__FILE__, __LINE__, conn->tcp);
    switch (conn->tcp->state)
    {
        case SYN_SENT:
            if (conn->tcp->nrtx == TCP_SYNMAXRTX)
            {
                err = ERR_CONN;
            }
            break;
        case ESTABLISHED:
            if (conn->tcp->nrtx == TCP_MAXRTX)
            {
                err = ERR_TIMEOUT;
            }
            break;
        default:
            break;
    }
    
    lwIP_EVENT_PARSE(find_socket(conn), err);
    return;
}

/**
 * TCP callback function if a connection (opened by tcp_connect/do_connect) has
 * been established (or reset by the remote host).
 *
 * @see tcp.h (struct tcp_pcb.connected) for parameters and return values
 */
static err_t do_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
    lwIP_netconn *conn = arg;
    err = ERR_OK;
    lwIP_REQUIRE_ACTION(conn, exit, err = ESP_ARG);
    conn->state = NETCONN_STATE_ESTABLISHED;
    conn->readbuf = ringbuf_new(TCP_SND_BUF);
    lwIP_REQUIRE_ACTION(conn->readbuf, exit, err = ESP_MEM);
    lwIP_EVENT_PARSE(find_socket(conn), ERR_OK);
exit:
    return err;
}

static void setup_tcp(lwIP_netconn *conn)
{
    struct tcp_pcb *pcb = NULL;
    sint8 ret = ERR_OK;
    lwIP_REQUIRE_ACTION(conn, exit, ret = ERR_ARG);
    lwIP_REQUIRE_ACTION(conn->tcp, exit, ret = ERR_ARG);

    pcb = conn->tcp;
    tcp_arg(pcb, conn);
    tcp_recv(pcb, recv_tcp);
    tcp_sent(pcb, sent_tcp);
    tcp_poll(pcb, poll_tcp, 4);
    tcp_err(pcb, err_tcp);
exit:
    return;
}

static err_t do_accepted(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    lwIP_netconn *newconn = NULL;
    lwIP_netconn *conn = arg;
    err = ERR_OK;

    //Avoid two TCP connections coming in simultaneously
    struct  tcp_pcb *pactive_pcb;
    int active_pcb_num=0;
    for(pactive_pcb = tcp_active_pcbs; pactive_pcb != NULL; pactive_pcb = pactive_pcb->next){
    	if (pactive_pcb->state == ESTABLISHED ||pactive_pcb->state == SYN_RCVD){
    		active_pcb_num++;
                if (active_pcb_num > MEMP_NUM_TCP_PCB){
                    ESP_LOG("%s %d active_pcb_number:%d\n",__FILE__, __LINE__,active_pcb_num);
                    return ERR_MEM;
                }
    	}
    }

    lwIP_REQUIRE_ACTION(conn, exit, err = ESP_ARG);
    /* We have to set the callback here even though
     * the new socket is unknown. conn->socket is marked as -1. */
    newconn = netconn_alloc(conn->type, newpcb);
    lwIP_REQUIRE_ACTION(conn, exit, err = ERR_MEM);
    newconn->tcp = newpcb;
    setup_tcp(newconn);
	newconn->state = NETCONN_STATE_ESTABLISHED;
    conn->acceptmbox = newconn;
    lwIP_EVENT_PARSE(find_socket(conn), ERR_OK);
exit:
    return err;
}

sint8 netconn_delete(lwIP_netconn *conn)
{
    sint8 error = ESP_OK;
    lwIP_REQUIRE_ACTION(conn, exit, error = ESP_ARG);
    tcp_recv(conn->tcp, NULL);
    error = tcp_close(conn->tcp);

    if (error != ERR_OK)
    {
        /* closing failed, try again later */
        tcp_recv(conn->tcp, recv_tcp);
    }
    else
    {
        /* closing succeeded */
        remove_tcp(conn);
    }
    free_netconn(conn);
exit:
    return error;
}

sint8 netconn_bind(lwIP_netconn *conn, ip_addr_t *addr, u16_t port)
{
    sint8 error = ESP_OK;
    lwIP_REQUIRE_ACTION(conn, exit, error = ESP_ARG);
    lwIP_REQUIRE_ACTION(addr, exit, error = ESP_ARG);
    lwIP_REQUIRE_ACTION(conn->tcp, exit, error = ESP_ARG);

    error = tcp_bind(conn->tcp, addr, port);
exit:
    return error;
}

sint8 netconn_connect(lwIP_netconn *conn, ip_addr_t *addr, u16_t port)
{
    sint8 error = ESP_OK;
    lwIP_REQUIRE_ACTION(conn, exit, error = ESP_ARG);
    lwIP_REQUIRE_ACTION(addr, exit, error = ESP_ARG);
    lwIP_REQUIRE_ACTION(conn->tcp, exit, error = ESP_ARG);

    setup_tcp(conn);
    error = tcp_connect(conn->tcp, addr, port, do_connected);
exit:
    return error;
}

err_t netconn_accept(lwIP_netconn *conn, lwIP_netconn **new_conn)
{
    err_t error = ESP_OK;
    lwIP_netconn *newconn = NULL;
    lwIP_REQUIRE_ACTION(conn, exit, error = ESP_ARG);
    lwIP_REQUIRE_ACTION(new_conn, exit, error = ESP_ARG);
    *new_conn = NULL;
    newconn = (lwIP_netconn *)conn->acceptmbox;
	conn->acceptmbox = NULL;
    lwIP_REQUIRE_ACTION(newconn, exit, error = ERR_CLSD);
    *new_conn = newconn;
exit:
    return error;
}

sint8 netconn_listen(lwIP_netconn *conn)
{
    sint8 error = ESP_OK;
    struct tcp_pcb *lpcb = NULL;

    lwIP_REQUIRE_ACTION(conn, exit, error = ESP_ARG);
    lwIP_REQUIRE_ACTION(conn->tcp, exit, error = ESP_ARG);

    setup_tcp(conn);
    lpcb = conn->tcp;
    conn->tcp = tcp_listen(conn->tcp);
    if (conn->tcp != NULL)
    {
        tcp_accept(conn->tcp, do_accepted);
    }
    else
    {
        conn->tcp = lpcb;
    }
exit:
    return error;
}

static int alloc_socket(lwIP_netconn *newconn, int accepted)
{
    int i = 0;
    lwIP_ASSERT(newconn);
    /* allocate a new socket identifier */
    for (i = 0; i < NUM_SOCKETS; ++i)
    {
        /* Protect socket array */
        if (!sockets[i].conn)
        {
            sockets[i].conn = newconn;
            return i;
        }
    }
    return -1;
}

static lwIP_sock *get_socket(int s)
{
    lwIP_sock *sock = NULL;

    if ((s < 0) || (s >= NUM_SOCKETS))
    {
        return NULL;
    }

    sock = &sockets[s];

    if (!sock->conn)
    {
        return NULL;
    }

    return sock;
}

int lwip_socket(int domain, int type, int protocol)
{
    lwIP_netconn *conn = NULL;
    int i = 0;
    switch (type)
    {
        case SOCK_STREAM:
            conn = (lwIP_netconn *)os_zalloc(sizeof(lwIP_netconn));
            lwIP_REQUIRE_ACTION(conn, exit, i = ESP_MEM);
            conn->tcp = tcp_new();
            lwIP_REQUIRE_ACTION(conn->tcp, exit, i = ESP_MEM);
            conn->socket = -1;
            break;
        default:
            return -1;
    }

    i = alloc_socket(conn, 0);
    if (i == -1)
    {
        goto exit;
    }
    conn->socket = i;
exit:
    if (i == -1)
    {
        if (conn->tcp)
            memp_free(MEMP_TCP_PCB,conn->tcp);
        if (conn)
            os_free(conn);
    }
    return i;
}

int lwip_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
    lwIP_sock *sock = NULL;
    ip_addr_t local_addr;
    uint16 local_port = 0;
    sint8 err = ERR_OK;
    const struct sockaddr_in *name_in = NULL;

    sock = get_socket(s);
    if (!sock)
    {
        return -1;
    }

    /* check size, familiy and alignment of 'name' */
    LWIP_ERROR("lwip_bind: invalid address",
               ((namelen == sizeof(struct sockaddr_in)) && ((name->sa_family) == AF_INET) && ((((mem_ptr_t)name) % 4) == 0)),
               return -1;);
    name_in = (const struct sockaddr_in *) (void*) name;

    inet_addr_to_ipaddr(&local_addr, &name_in->sin_addr);
    local_port = name_in->sin_port;

    err = netconn_bind(sock->conn, &local_addr, ntohs(local_port));

    if (err != ESP_OK)
    {
        ESP_LOG("%s %d, err=%d\n", __FILE__, __LINE__, err);
        return -1;
    }
    ESP_LOG("%s %d, %d succeeded\n", __FILE__, __LINE__, s);
    return ERR_OK;
}

int lwip_connect(int s, const struct sockaddr *name, socklen_t namelen)
{
    lwIP_sock *sock = NULL;
    err_t err = ERR_OK;
    ip_addr_t remote_addr;
    u16_t remote_port = 0;
    const struct sockaddr_in *name_in = NULL;

    sock = get_socket(s);
    if (!sock)
    {
        return -1;
    }

    /* check size, familiy and alignment of 'name' */
    LWIP_ERROR("lwip_connect: invalid address",
               ((namelen == sizeof(struct sockaddr_in)) && ((name->sa_family) == AF_INET) && ((((mem_ptr_t)name) % 4) == 0)),
               return -1;);
    name_in = (const struct sockaddr_in *) (void*) name;

    inet_addr_to_ipaddr(&remote_addr, &name_in->sin_addr);
    remote_port = name_in->sin_port;

    err = netconn_connect(sock->conn, &remote_addr, ntohs(remote_port));
    if (err != ERR_OK)
    {
        ESP_LOG("lwip_connect(%d) failed, err=%d\n", s, err);
        return -1;
    }

    ESP_LOG("lwip_connect(%d) succeeded\n", s);
    return ERR_OK;
}

int lwip_fcntl(int s, int cmd, int val)
{
    lwIP_sock *sock = get_socket(s);
    int ret = -1;

    if (!sock || !sock->conn)
    {
        return -1;
    }

    switch (cmd)
    {
        case F_GETFL:
            ret = netconn_is_nonblocking(sock->conn) ? O_NONBLOCK : 0;
            break;
        case F_SETFL:
            if ((val & ~O_NONBLOCK) == 0)
            {
                /* only O_NONBLOCK, all other bits are zero */
                netconn_set_nonblocking(sock->conn, val & O_NONBLOCK);
                ret = 0;
            }
            break;
        default:
            LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_fcntl(%d, UNIMPL: %d, %d)\n", s, cmd, val));
            break;
    }
    return ret;
}

uint32_t lwip_getul(char *str)
{
    uint32 ret = 0;

    while (isdigit(*str))
    {
        ret = ret * 10 + *str++ - '0';
    }

    return ret;
}

int lwip_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    lwIP_sock *sock = NULL;
    err_t err = ERR_OK;
    lwIP_netconn *newconn = NULL;
    int newsock = -1;
    sock = get_socket(s);
    if (!sock)
    {
        return -1;
    }

    /* wait for a new connection */
    err = netconn_accept(sock->conn, &newconn);
    lwIP_REQUIRE_NOERROR(err, exit);
    newsock = alloc_socket(newconn, 0);
    if (newsock == -1)
    {
        goto exit;
    }
    newconn->socket = newsock;
exit:
    if (newsock == -1)
    {
        netconn_delete(newconn);
    }
    return newsock;
}

int lwip_listen(int s, int backlog)
{
    lwIP_sock *sock = NULL;
    err_t err = ERR_OK;
    sock = get_socket(s);
    if (!sock)
    {
        return -1;
    }
    err = netconn_listen(sock->conn);
    if (err != ERR_OK)
    {
        ESP_LOG("lwip_connect(%d) failed, err=%d\n", s, err);
        return -1;
    }

    ESP_LOG("lwip_connect(%d) succeeded\n", s);

    return ERR_OK;
}

int lwip_recvfrom(int s, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    lwIP_sock *sock = NULL;
    size_t bytes_used = 0;
    int is_tcp = 0;
    lwIP_ASSERT(mem);

    sock = get_socket(s);
    if (!sock)
    {
        return -1;
    }

    if (sock->conn != NULL)
    {
        if (sock->conn->state == NETCONN_STATE_ESTABLISHED)
        {
            bytes_used = ringbuf_bytes_used(sock->conn->readbuf);
            if (bytes_used != 0)
            {
                if (len > bytes_used)
                {
                    len = bytes_used;
                }
                ringbuf_memcpy_from(mem, sock->conn->readbuf, len);
                return len;
            }
            else
            {
                return 0;
            }
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return -1;
    }
}

int lwip_read(int s, void *mem, size_t len)
{
    return lwip_recvfrom(s, mem, len, 0, NULL, NULL);
}

int lwip_recv(int s, void *mem, size_t len, int flags)
{
    return lwip_recvfrom(s, mem, len, flags, NULL, NULL);
}

int lwip_send(int s, const void *data, size_t size, int flags)
{
    lwIP_sock *sock = NULL;
    size_t bytes_used = 0;
    err_t Err = ERR_OK;

	lwIP_ASSERT(data);
    sock = get_socket(s);
    if (!sock)
    {
        return -1;
    }
	
    if (tcp_sndbuf(sock->conn->tcp) < size)
    {
        bytes_used = tcp_sndbuf(sock->conn->tcp);
    } else{
		bytes_used = size;
	}

    if (bytes_used > 2 * sock->conn->tcp->mss)
    {
        bytes_used = 2 * sock->conn->tcp->mss;
    }

    do
    {
        Err = tcp_write(sock->conn->tcp, data, bytes_used, 1);
        if (Err == ERR_MEM)
            size /= 2;
    }
    while (Err == ERR_MEM && size > 1);

    if (Err == ERR_OK)
    {
        Err = tcp_output(sock->conn->tcp);
    } else{
		size = Err; 
	}
    
    return size;
}

int lwip_close(int s)
{
    lwIP_sock *sock = NULL;
    int err = 0;

    sock = get_socket(s);
    if (!sock)
    {
        return -1;
    }

    /*Do not set callback function when tcp->state is LISTEN.
    Avoid memory overlap when conn->tcp changes from
    struct tcp_bcb to struct tcp_pcb_listen after lwip_listen.*/
    if (sock->conn->tcp->state != LISTEN)
    {
        if (sock->conn->state != NETCONN_STATE_ERROR){
            tcp_recv(sock->conn->tcp, NULL);
            err = tcp_close(sock->conn->tcp);

            if (err != ERR_OK)
            {
                /* closing failed, try again later */
                tcp_recv(sock->conn->tcp, recv_tcp);
                return -1;
            }
        }
        /* closing succeeded */
        remove_tcp(sock->conn);
    } else {
        tcp_close(sock->conn->tcp);
    }
    free_netconn(sock->conn);
    free_socket(sock);
    return ERR_OK;
}

int lwip_write(int s, const void *data, size_t size)
{
    lwIP_sock *sock = NULL;
    int is_tcp = 0;
    size_t bytes_free = 0;

    sock = get_socket(s);
    if (!sock)
    {
        return -1;
    }

    if (sock->conn != NULL)
    {
        switch (sock->conn->state)
        {
            case NETCONN_STATE_ESTABLISHED:                
               return lwip_send(s, data, size, 0);
            default:
                return -1;
        }
    }
    else
    {
        return -1;
    }
}

static int
lwip_getaddrname(int s, struct sockaddr *name, socklen_t *namelen, u8_t local)
{
    lwIP_sock *sock = NULL;
    struct sockaddr_in sin;
    ip_addr_t naddr;
    lwIP_ASSERT(name);
    lwIP_ASSERT(namelen);

    sock = get_socket(s);
    if (!sock)
    {
        return -1;
    }

    os_memset(&sin, 0, sizeof(sin));
    sin.sin_len = sizeof(sin);
    sin.sin_family = AF_INET;

    /* get the IP address and port */
    netconn_getaddr(sock->conn, &naddr, &sin.sin_port, local);
    sin.sin_port = htons(sin.sin_port);
    inet_addr_from_ipaddr(&sin.sin_addr, &naddr);

    if (*namelen > sizeof(sin))
    {
        *namelen = sizeof(sin);
    }

    MEMCPY(name, &sin, *namelen);

    return 0;
}

int lwip_getpeername(int s, struct sockaddr *name, socklen_t *namelen)
{
    return lwip_getaddrname(s, name, namelen, 0);
}

int lwip_getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
    return lwip_getaddrname(s, name, namelen, 1);
}

int lwip_getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen)
{
    lwIP_sock *sock = NULL;
    err_t err = ERR_OK;

    lwIP_REQUIRE_ACTION(optval, exit, err = ESP_ARG);
    lwIP_REQUIRE_ACTION(optlen, exit, err = ESP_ARG);

    sock = get_socket(s);
    lwIP_REQUIRE_ACTION(sock, exit, err = ESP_MEM);
    switch (level)
    {
        /* Level: SOL_SOCKET */
        case SOL_SOCKET:
            switch (optname)
            {
                /* The option flags */
                case SO_ACCEPTCONN:
                case SO_BROADCAST:
                case SO_KEEPALIVE:
#if SO_REUSE
                case SO_REUSEADDR:
                case SO_REUSEPORT:
#endif /* SO_REUSE */
                    *(int*)optval = sock->conn->tcp->so_options & optname;
                    break;
                case SO_TYPE:
                    switch (NETCONNTYPE_GROUP(sock->conn->type))
                    {
                        case NETCONN_TCP:
                            *(int*)optval = SOCK_STREAM;
                            break;
                        case NETCONN_UDP:
                            *(int*)optval = SOCK_DGRAM;
                            break;
                        default:
                            *(int*)optval = sock->conn->type;
                            break;
                    }
                    break;
                    break;
            }
            break;
        /* Level: IPPROTO_IP */
        case IPPROTO_IP:
            break;
#if LWIP_TCP
        /* Level: IPPROTO_TCP */
        case IPPROTO_TCP:
            if (*optlen < sizeof(int))
            {
                err = ESP_ARG;
                lwIP_REQUIRE_NOERROR(err, exit);
            }

            /* If this is no TCP socket, ignore any options. */
            if (sock->conn->type != NETCONN_TCP)
            {
                err = ESP_ARG;
                lwIP_REQUIRE_NOERROR(err, exit);
            }

            switch (optname)
            {
                case TCP_NODELAY:
                case TCP_KEEPALIVE:
                    *(int*)optval = (int)sock->conn->tcp->keep_idle;
                    break;
#if LWIP_TCP_KEEPALIVE
                case TCP_KEEPIDLE:
                    *(int*)optval = (int)(sock->conn->tcp->keep_idle/1000);
                    break;
                case TCP_KEEPINTVL:
                    *(int*)optval = (int)(sock->conn->tcp->keep_intvl/1000);
                    break;
                case TCP_KEEPCNT:
                    *(int*)optval = (int)sock->conn->tcp->keep_cnt;
#endif /* LWIP_TCP_KEEPALIVE */
                    break;

                default:
                    err = ESP_ARG;
                    lwIP_REQUIRE_NOERROR(err, exit);
                    break;
            } /* switch (optname) */
            break;
#endif /* LWIP_TCP */
        default:
            err = ESP_ARG;
            lwIP_REQUIRE_NOERROR(err, exit);
            break;
    }

exit:
    return err;
}

int lwip_setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen)
{
    lwIP_sock *sock = NULL;
    err_t err = ERR_OK;
    lwIP_REQUIRE_ACTION(optval, exit, err = ESP_ARG);

    sock = get_socket(s);
    lwIP_REQUIRE_ACTION(sock, exit, err = ESP_MEM);
    lwIP_REQUIRE_ACTION(sock->conn, exit, err = ESP_MEM);
    lwIP_REQUIRE_ACTION(sock->conn->tcp, exit, err = ESP_MEM);
    switch (level)
    {
        /* Level: SOL_SOCKET */
        case SOL_SOCKET:
            switch (optname)
            {
                case SO_KEEPALIVE:
                    if (optlen < sizeof(int))
                    {
                        err = ESP_ARG;
                        lwIP_REQUIRE_NOERROR(err, exit);
                    }
                    if (*(int*)optval)
                    {
                        sock->conn->tcp->so_options |= optname;
                    }
                    else
                    {
                        sock->conn->tcp->so_options &= ~optname;
                    }
                    break;
            }
            break;
        /* Level: IPPROTO_IP */
        case IPPROTO_IP:
            break;
        /* Level: IPPROTO_TCP */
        case IPPROTO_TCP:
            if (optlen < sizeof(int))
            {
                err = ESP_ARG;
                lwIP_REQUIRE_NOERROR(err, exit);
            }

            /* If this is no TCP socket, ignore any options. */
            if (NETCONNTYPE_GROUP(sock->conn->type) != NETCONN_TCP)
            {
                err = ESP_ARG;
                lwIP_REQUIRE_NOERROR(err, exit);
            }
            switch (optname)
            {
                case TCP_KEEPALIVE:
                    sock->conn->tcp->keep_idle = (u32_t) (*(int*) optval);
                    break;

#if LWIP_TCP_KEEPALIVE
                case TCP_KEEPIDLE:
                    sock->conn->tcp->keep_idle = 1000 * (u32_t) (*(int*) optval);
                    break;
                case TCP_KEEPINTVL:
                    sock->conn->tcp->keep_intvl = 1000 * (u32_t) (*(int*) optval);
                    break;
                case TCP_KEEPCNT:
                    sock->conn->tcp->keep_cnt = (u32_t) (*(int*) optval);
                    break;
#endif /* LWIP_TCP_KEEPALIVE */
                default:
                    err = ESP_ARG;
                    lwIP_REQUIRE_NOERROR(err, exit);
                    break;
            }
            break;
        /* UNDEFINED LEVEL */
        default:
            err = ESP_ARG;
            lwIP_REQUIRE_NOERROR(err, exit);
            break;
    }

exit:
    return err;
}
