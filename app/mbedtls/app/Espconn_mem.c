#include "c_types.h"
#include "mem.h"
#include "user_interface.h"

void *espconn_memzalloc(size_t size, char* file, int line)
{
	void *mp = NULL;
	if ((mp = (void*)os_malloc(size)) == NULL){

	} else{
		//os_printf("%s %d %p %d\n",file, line, mp, size);
		os_memset(mp, 0, size);
	}
	return mp;
}

void espconn_memfree(void *fp, char* file, int line)
{
	//os_printf("%s %d %p\n",file, line, fp);
	os_free(fp);
	fp = NULL;
}

void *espconn_memcpy(void *dst, const void *src, size_t size, char *file, int line)
{
	char *psrc = NULL;
	char *pdst = NULL;
	
	if(NULL == dst || NULL == src)
	{
		return NULL;
	}
	//os_printf("%s %d %p %p %d\n",file, line, dst, src, size);
	if((src < dst) && (char *)src + size > (char *)dst) 
	{
		psrc = (char *)src + size - 1;
		pdst = (char *)dst + size - 1;
		while(size--)
		{
			*pdst-- = *psrc--;
		}
	}
	else
	{
		psrc = (char *)src;
		pdst = (char *)dst;
		while(size--)
		{
			*pdst++ = *psrc++;
		}
	}
	
	return dst;

}

void *espconn_memcalloc(size_t count, size_t size)
{
	void *cp = NULL;
	cp = espconn_memzalloc(count * size, __FILE__, __LINE__);
	return cp;
}

void espconn_memFree(void *fp)
{
	espconn_memfree(fp, __FILE__, __LINE__);
}

void *espconn_memCpy(void *dst, const void *src, size_t size)
{
	return espconn_memcpy(dst, src, size, __FILE__, __LINE__);
}

