#ifndef __USER_VERSION_H__
#define __USER_VERSION_H__

#include "version.h"	/* ESP firmware header */
#include <buildinfo.h>

#define NODE_VERSION_MAJOR	ESP_SDK_VERSION_MAJOR
#define NODE_VERSION_MINOR	ESP_SDK_VERSION_MINOR
#define NODE_VERSION_REVISION	ESP_SDK_VERSION_PATCH
#define NODE_VERSION_INTERNAL   0

#define NODE_VERSION_STR(x)	#x
#define NODE_VERSION_XSTR(x)	NODE_VERSION_STR(x)

# define NODE_VERSION		"NodeMCU " ESP_SDK_VERSION_STRING "." NODE_VERSION_XSTR(NODE_VERSION_INTERNAL) " " NODE_VERSION_LONG
// Leave the space after # in the line above. It busts replacement of NODE_VERSION in the docker build which is not needed anymore with this PR.
// Can be removed when the script is adapted

#ifndef BUILD_DATE
#define BUILD_DATE		BUILDINFO_BUILD_DATE
#endif

extern char SDK_VERSION[];

#endif	/* __USER_VERSION_H__ */
