#ifndef __USER_VERSION_H__
#define __USER_VERSION_H__

#include <buildinfo.h>

#define NODE_VERSION_MAJOR		0U
#define NODE_VERSION_MINOR		0U
#define NODE_VERSION_REVISION	0U
#define NODE_VERSION_INTERNAL   0U

#define NODE_VERSION	"NodeMCU ESP32"
#ifndef BUILD_DATE
#define BUILD_DATE	  BUILDINFO_BUILD_DATE
#endif

#define SDK_VERSION IDF_VER

#endif	/* __USER_VERSION_H__ */

