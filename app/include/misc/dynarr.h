#ifndef __DYNARR_H__
#define __DYNARR_H__
#include "user_interface.h"
#include "c_stdio.h"
#include "c_stdlib.h"

//#define DYNARR_DEBUG
//#define DYNARR_ERROR


typedef struct _dynarr{
  void* data_ptr;
  size_t used;
  size_t array_size;
  size_t data_size;
} dynarr_t;

bool dynarr_init(dynarr_t* array_ptr, size_t array_size, size_t data_size);
bool dynarr_resize(dynarr_t* array_ptr, size_t elements_to_add);
bool dynarr_remove(dynarr_t* array_ptr, void* element_ptr);
bool dynarr_add(dynarr_t* array_ptr, void* data_ptr, size_t data_size);
bool dynarr_boundaryCheck(dynarr_t* array_ptr, void* element_ptr);
bool dynarr_free(dynarr_t* array_ptr);


#if 0 || defined(DYNARR_DEBUG) || defined(NODE_DEBUG)
#define DYNARR_DBG(fmt, ...) c_printf("\n DYNARR_DBG(%s): "fmt"\n", __FUNCTION__, ##__VA_ARGS__)
#else
#define DYNARR_DBG(...)

#endif

#if 0 || defined(DYNARR_ERROR) || defined(NODE_ERROR)
#define DYNARR_ERR(fmt, ...) c_printf("\n DYNARR: "fmt"\n", ##__VA_ARGS__)
#else
#define DYNARR_ERR(...)

#endif

#endif // __DYNARR_H__
