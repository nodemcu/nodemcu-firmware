/* str.c -- strings to be used in the CoAP library
 *
 * Copyright (C) 2010,2011 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see
 * README for terms of use. 
 */

#include "c_stdlib.h"
#include "c_types.h"
 
#include "str.h"

str * coap_new_string(size_t size) {
  str *s = (str *)c_malloc(sizeof(str) + size + 1);
  if ( !s ) {
    return NULL;
  }

  c_memset(s, 0, sizeof(str));
  s->s = ((unsigned char *)s) + sizeof(str);
  return s;
}

void coap_delete_string(str *s) {
  c_free(s);
}

