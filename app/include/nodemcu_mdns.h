#ifndef _NODEMCU_MDNS_H
#define _NODEMCU_MDNS_H

struct nodemcu_mdns_info {
	const char *host_name;
	const char *host_desc;
	const char *service_name;
	uint16 service_port;
	const char *txt_data[10];
};

void nodemcu_mdns_close(void);
bool nodemcu_mdns_init(struct nodemcu_mdns_info *);


#endif
