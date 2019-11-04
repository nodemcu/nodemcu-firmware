#ifndef LUA_USE_MODULES_HTTPD_
#define LUA_USE_MODULES_HTTPD_

typedef enum net_type {
	TYPE_TCP_SERVER = 0, TYPE_TCP_CLIENT, TYPE_UDP_SOCKET
} net_type;

typedef enum service_type {
	NONE = 0, HTTP = 1
} service_type;

#define TYPE_TCP_SERVER 0
#define TYPE_TCP_CLIENT 1
#define TYPE_UDP_SOCKET 2

#define TYPE_TCP TYPE_TCP_CLIENT
#define TYPE_UDP TYPE_UDP_SOCKET

typedef struct {
	enum net_type type;
	int self_ref;
	union {
		struct tcp_pcb *tcp_pcb;
		struct udp_pcb *udp_pcb;
		void *pcb;
	};
	union {
		struct {
			int cb_accept_ref;
			int timeout;
		} server;
		struct {
			int wait_dns;
			int cb_dns_ref;
			int cb_receive_ref;
			int cb_sent_ref;
			// Only for TCP:
			int hold;
			int cb_connect_ref;
			int cb_disconnect_ref;
			int cb_reconnect_ref;
		} client;
	};
	int service; // service type http or raw
	int ht_head; // http header completed -> frames received (0 is header)
	int fd; // fd to write frames to, used for writing received data / files
	int req_left; // Remaining bytes to recv (http payload)
	int refT; // reference to the registry
	int state;
} lnet_userdata;

//typedef llwhttp_userdata lnet_userdata;

lnet_userdata *net_get_udata_s( lua_State *L, int stack );

void httpd_recv_cb(lnet_userdata *ud, struct pbuf *p, ip_addr_t *addr, u16_t port);

int lwip_lua_checkerr (lua_State *L, err_t err);
int httpd_send_fileX(lua_State *L);
int http_send_H(lua_State *L);
int http_processReq(lua_State *L);

#endif /* LUA_USE_MODULES_HTTPD_ */
