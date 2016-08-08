#ifndef __USER_VERSION_H__
#define __USER_VERSION_H__

#define NODE_VERSION_MAJOR		1U
#define NODE_VERSION_MINOR		5U
#define NODE_VERSION_REVISION	4U
#define NODE_VERSION_INTERNAL   1U

#define NODE_VERSION	"NodeMCU 1.5.4.1"
#ifndef BUILD_DATE
#define BUILD_DATE	  "unspecified"
#endif

extern char SDK_VERSION[];

#endif	/* __USER_VERSION_H__ */
