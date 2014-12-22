#include "c_stdlib.h"
#include "c_stdio.h"
#include "c_types.h"
#include "c_string.h"
#include "user_interface.h"

// const char *lua_init_value = "print(\"Hello world\")";
const char *lua_init_value = "@init.lua";

// int ICACHE_FLASH_ATTR c_abs(int x){
// 	return x>0?x:0-x;
// }
// void ICACHE_FLASH_ATTR c_exit(int e){
// }
const char *ICACHE_FLASH_ATTR c_getenv(const char *__string){
	if (c_strcmp(__string, "LUA_INIT") == 0){
		return lua_init_value;
	}
	return NULL;
}

// make sure there is enough memory before real malloc, otherwise malloc will panic and reset
void *ICACHE_FLASH_ATTR c_malloc(size_t __size){
	if(__size>system_get_free_heap_size()){
		NODE_ERR("malloc: not enough memory\n");
		return NULL;
	}
	return (void *)os_malloc(__size);
}

void *ICACHE_FLASH_ATTR c_zalloc(size_t __size){
	if(__size>system_get_free_heap_size()){
		NODE_ERR("zalloc: not enough memory\n");
		return NULL;
	}
	return (void *)os_zalloc(__size);
}

void ICACHE_FLASH_ATTR c_free(void *p){
	// NODE_ERR("free1: %d\n", system_get_free_heap_size());
	os_free(p);
	// NODE_ERR("-free1: %d\n", system_get_free_heap_size());
}


// int	ICACHE_FLASH_ATTR c_rand(void){
// }
// void ICACHE_FLASH_ATTR c_srand(unsigned int __seed){
// }

// int	ICACHE_FLASH_ATTR c_atoi(const char *__nptr){
// }
// double	ICACHE_FLASH_ATTR c_strtod(const char *__n, char **__end_PTR){
// }
// long	ICACHE_FLASH_ATTR c_strtol(const char *__n, char **__end_PTR, int __base){
// }
// unsigned long ICACHE_FLASH_ATTR c_strtoul(const char *__n, char **__end_PTR, int __base){
// }
// long long ICACHE_FLASH_ATTR c_strtoll(const char *__n, char **__end_PTR, int __base){
// }
