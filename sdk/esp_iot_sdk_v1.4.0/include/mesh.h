#ifndef __LWIP_API_MESH_H__
#define __LWIP_API_MESH_H__

#include "ip_addr.h"
#include "user_interface.h"
#include "espconn.h"

typedef void (* espconn_mesh_callback)();

enum mesh_type {
	MESH_CLOSE = 0,
    MESH_LOCAL,
    MESH_ONLINE,
    MESH_NONE = 0xFF
};

enum mesh_status {
    MESH_DISABLE = 0,
    MESH_WIFI_CONN,
    MESH_NET_CONN,
    MESH_LOCAL_AVAIL,
    MESH_ONLINE_AVAIL
};

enum mesh_node_type {
	MESH_NODE_PARENT = 0,
	MESH_NODE_CHILD,
	MESH_NODE_ALL
};

bool espconn_mesh_local_addr(struct ip_addr *ip);
bool espconn_mesh_get_node_info(enum mesh_node_type type,
		                        uint8_t **info, uint8_t *count);
bool espconn_mesh_get_router(struct station_config *router);
bool espconn_mesh_set_router(struct station_config *router);
bool espconn_mesh_encrypt_init(AUTH_MODE mode, uint8_t *passwd, uint8_t passwd_len);
bool espconn_mesh_set_ssid_prefix(uint8_t *prefix, uint8_t prefix_len);
bool espconn_mesh_set_max_hops(uint8_t max_hops);

char * espconn_json_find_section(const char *pbuf, u16 len, const char *section);

sint8 espconn_mesh_connect(struct espconn *usr_esp);
sint8 espconn_mesh_disconnect(struct espconn *usr_esp);
sint8 espconn_mesh_get_status();
sint8 espconn_mesh_sent(struct espconn *usr_esp, uint8 *pdata, uint16 len);

uint8 espconn_mesh_get_max_hops();
uint8 espconn_mesh_layer(struct ip_addr *ip);
uint32_t user_json_get_value(const char *pbuffer, uint16_t buf_len,
		                     const uint8_t *json_key);

void espconn_mesh_enable(espconn_mesh_callback enable_cb, enum mesh_type type);
void espconn_mesh_disable(espconn_mesh_callback disable_cb);
void espconn_mesh_init();
void espconn_mesh_init_group_list(uint8_t *dev_mac, uint16_t dev_count);
void espconn_mesh_set_dev_type(uint8_t dev_type);
void espconn_mesh_setup_timer(os_timer_t *timer, uint32_t time,
                              os_timer_func_t cb, void *arg, bool repeat);

#endif

