#ifndef _NODEMCU_MDNS_H
#define _NODEMCU_MDNS_H

struct nodemcu_mdns_info {
	char *host_name;
	char *host_desc;
	char *service_name;
	uint16 server_port;
	char *txt_data[10];
};

void nodemcu_mdns_close(void);
void nodemcu_mdns_init(struct nodemcu_mdns_info *);


#endif
