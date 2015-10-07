#ifndef __USER_VERSION_H__
#define __USER_VERSION_H__

#define NODE_VERSION_MAJOR		1U
#define NODE_VERSION_MINOR		4U
#define NODE_VERSION_REVISION	0U
#define NODE_VERSION_INTERNAL   0U

#define NODE_VERSION	"NodeMCU 1.4.0"
#ifndef BUILD_DATE
#define BUILD_DATE	  "20151006"
#endif

extern char SDK_VERSION[];

#endif	/* __USER_VERSION_H__ */
