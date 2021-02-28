/* str.c -- strings to be used in the CoAP library
 *
 * Copyright (C) 2010,2011 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see
 * README for terms of use.
 */

#include <stdlib.h>
#include <stddef.h>

#include "str.h"

str * coap_new_string(size_t size) {
  str *s = (str *)malloc(sizeof(str) + size + 1);
  if ( !s ) {
    return NULL;
  }

  memset(s, 0, sizeof(str));
  s->s = ((unsigned char *)s) + sizeof(str);
  return s;
}

void coap_delete_string(str *s) {
  free(s);
}

