#ifndef __MEM_H__
#define __MEM_H__

#ifndef MEMLEAK_DEBUG
#define os_free(s)        vPortFree(s, "", 0)
#define os_malloc(s)      pvPortMalloc(s, "", 0)
#define os_calloc(s)      pvPortCalloc(s, "", 0);
#define os_realloc(p, s)  pvPortRealloc(p, s, "", 0)
#define os_zalloc(s)      pvPortZalloc(s, "", 0)
#else
#define os_free(s) \
do{\
	const char *file = mem_debug_file;\
    vPortFree(s, file, __LINE__);\
}while(0)

#define os_malloc(s) ({const char *file = mem_debug_file; pvPortMalloc(s, file, __LINE__);})

#define os_calloc(s) ({const char *file = mem_debug_file; pvPortCalloc(s, file, __LINE__);})

#define os_realloc(p, s) ({const char *file = mem_debug_file; pvPortRealloc(p, s, file, __LINE__);})

#define os_zalloc(s) ({const char *file = mem_debug_file; pvPortZalloc(s, file, __LINE__);})
#endif

#endif

