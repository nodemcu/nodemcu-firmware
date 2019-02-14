#ifndef __USER_VERSION_H__
#define __USER_VERSION_H__

#include "version.h"	/* ESP firmware header */

#define NODE_VERSION_MAJOR	ESP_SDK_VERSION_MAJOR
#define NODE_VERSION_MINOR	ESP_SDK_VERSION_MINOR
#define NODE_VERSION_REVISION	ESP_SDK_VERSION_PATCH
#define NODE_VERSION_INTERNAL   0

#define NODE_VERSION_STR(x)	#x
#define NODE_VERSION_XSTR(x)	NODE_VERSION_STR(x)

#define NODE_VERSION		"NodeMCU " ESP_SDK_VERSION_STRING "." NODE_VERSION_XSTR(NODE_VERSION_INTERNAL) " built with Docker provided by frightanic.com\n\tbranch: pwm2\n\tcommit: 27e9e6c085aaac8b7e78b7c1d9cd5afe39428ba0\n\tSSL: false\n\tBuild type: float\n\tLFS: disabled\n\tmodules: adc,bit,dht,file,gpio,i2c,mqtt,net,node,ow,spi,tmr,uart,wifi\n"

#ifndef BUILD_DATE
#define BUILD_DATE		"created on 2019-02-14 14:15\n"
#endif

extern char SDK_VERSION[];

#endif	/* __USER_VERSION_H__ */
