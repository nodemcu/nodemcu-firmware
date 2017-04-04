#include "misc/dynarr.h"

#define ARRAY_PTR_CHECK if(array_ptr == NULL || array_ptr->data_ptr == NULL){\
    /**/DYNARR_DBG("array not initialized");\
    return false; \
  }

bool dynarr_init(dynarr_t* array_ptr, size_t array_size, size_t data_size){
  if(array_ptr == NULL || data_size == 0 || array_size == 0){
    /**/DYNARR_DBG("Invalid parameter: array_ptr(%p) data_size(%u) array_size(%u)", array_ptr, data_size, array_size);
    return false;
  }
  if(array_ptr->data_ptr != NULL ){
    /**/DYNARR_DBG("Array already initialized: array_ptr->data_ptr=%p", array_ptr->data_ptr);
    return false;
  }
  /**/DYNARR_DBG("Array parameters:\n\t\t\tarray_size(%u)\n\t\t\tdata_size(%u)\n\t\t\ttotal size(bytes):%u", array_size, data_size, (array_size * data_size));

  void* temp_array = c_zalloc(array_size * data_size);
  if(temp_array == NULL){
    /**/DYNARR_ERR("malloc FAIL! req:%u free:%u", (array_size * data_size), system_get_free_heap_size());
    return false;
  }

  array_ptr->data_ptr = temp_array;
  array_ptr->array_size = array_size;
  array_ptr->data_size = data_size;
  array_ptr->used = 0;

  return true;
}

bool dynarr_resize(dynarr_t* array_ptr, size_t elements_to_add){
  ARRAY_PTR_CHECK;

  if(elements_to_add <= 0){
    /**/DYNARR_DBG("Invalid qty: elements_to_add=%u", elements_to_add);
    return false;
  }

  size_t new_array_size = array_ptr->array_size + elements_to_add;

  /**/DYNARR_DBG("old size=%u\tnew size=%u\tmem used=%u",
      array_ptr->array_size, new_array_size, (new_array_size * array_ptr->data_size));

  void* temp_array_p = c_realloc(array_ptr->data_ptr, new_array_size * array_ptr->data_size);
  if(temp_array_p == NULL){
    /**/DYNARR_ERR("malloc FAIL! req:%u free:%u", (new_array_size * array_ptr->data_size), system_get_free_heap_size());
    return false;
  }

  array_ptr->data_ptr = temp_array_p;

  size_t prev_size = array_ptr->array_size;

  array_ptr->array_size = new_array_size;

  //set memory to 0 for newly added array elements
  memset((uint8*) array_ptr->data_ptr + (prev_size * array_ptr->data_size), 0, (elements_to_add * array_ptr->data_size));

  /**/DYNARR_DBG("Array successfully resized");
  return true;
}

bool dynarr_remove(dynarr_t* array_ptr, void* element_to_remove){
  ARRAY_PTR_CHECK;

  uint8* element_ptr = element_to_remove;
  uint8* data_ptr = array_ptr->data_ptr;

  if(dynarr_boundaryCheck(array_ptr, element_to_remove) == FALSE){
    return false;
  }

  //overwrite element to be removed by shifting all elements to the left
  memmove(element_ptr, element_ptr + array_ptr->data_size,  (array_ptr->array_size - 1) * array_ptr->data_size - (element_ptr - data_ptr));

  //clear newly freed element
  memset(data_ptr + ((array_ptr->array_size-1) * array_ptr->data_size), 0, array_ptr->data_size);

  //decrement array used since we removed an element
  array_ptr->used--;
  /**/DYNARR_DBG("element(%p) removed from array", element_ptr);
  return true;
}

bool dynarr_add(dynarr_t* array_ptr, void* data_ptr, size_t data_size){
  ARRAY_PTR_CHECK;

  if(data_size != array_ptr->data_size){
    /**/DYNARR_DBG("Invalid data size: data_size(%u) != arr->data_size(%u)", data_size, array_ptr->data_size);
    return false;
  }

  if(array_ptr->array_size == array_ptr->used){
    if(!dynarr_resize(array_ptr, (array_ptr->array_size/2))){
      return false;
    }
  }
  memcpy(((uint8*)array_ptr->data_ptr + (array_ptr->used * array_ptr->data_size)), data_ptr, array_ptr->data_size);

  array_ptr->used++;
  return true;
}

bool dynarr_boundaryCheck(dynarr_t* array_ptr, void* element_to_check){
  ARRAY_PTR_CHECK;

  uint8* data_ptr = array_ptr->data_ptr;
  uint8* element_ptr = element_to_check;

  if(element_ptr < data_ptr || ((element_ptr - data_ptr) / array_ptr->data_size) > array_ptr->array_size - 1){
    /**/DYNARR_DBG("element_ptr(%p) out of bounds: first element ptr:%p last element ptr:%p",
        element_ptr, data_ptr, data_ptr + ((array_ptr->array_size - 1) * array_ptr->data_size));
    return false;
  }
  return true;
}

bool dynarr_free(dynarr_t* array_ptr){
  ARRAY_PTR_CHECK;

  c_free(array_ptr->data_ptr);

  array_ptr->data_ptr=NULL;
  array_ptr->array_size = array_ptr->used = 0;

  /**/DYNARR_DBG("array freed");
  return true;
}

