#ifndef __USER_VERSION_H__
#define __USER_VERSION_H__

#define NODE_VERSION_MAJOR		0U
#define NODE_VERSION_MINOR		0U
#define NODE_VERSION_REVISION	0U
#define NODE_VERSION_INTERNAL   0U

#define NODE_VERSION	"NodeMCU ESP32" " built with Docker provided by frightanic.com\n\tbranch: dev-esp32\n\tcommit: f4887bf134235c05e6c9b2efad370e6d5018f91a\n\tSSL: true\n\tmodules: -\n"
#ifndef BUILD_DATE
#define BUILD_DATE	  "created on 2020-05-30 18:01\n"
#endif

#define SDK_VERSION "IDF"

#endif	/* __USER_VERSION_H__ */
