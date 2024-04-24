/*
 * Copyright 2016 Dius Computing Pty Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 * - Neither the name of the copyright holders nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @author Johny Mattsson <jmattsson@dius.com.au>
 */
#ifndef _NODEMCU_WIFI_H_
#define _NODEMCU_WIFI_H_

#include <stdint.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "lua.h"

// Shared sta/ap macros

#define DEFAULT_SAVE false
#define SET_SAVE_MODE(save) \
  do { esp_err_t storage_err = \
    esp_wifi_set_storage (save ? WIFI_STORAGE_FLASH : WIFI_STORAGE_RAM); \
    if (storage_err != ESP_OK) \
      return luaL_error (L, "failed to update wifi storage, code %d", \
         storage_err); \
  } while(0)


// Shared event handling support

typedef void (*fill_cb_arg_fn) (lua_State *L, const void *data);
typedef struct
{
  const char *name;
  const esp_event_base_t *event_base_ptr;
  int32_t  event_id;
  fill_cb_arg_fn fill_cb_arg;
} event_desc_t;

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

int wifi_event_idx_by_name (const event_desc_t *table, unsigned n, const char *name);
int wifi_event_idx_by_id (const event_desc_t *table, unsigned n, esp_event_base_t base, int32_t id);

int wifi_on (lua_State *L, const event_desc_t *table, unsigned n, int *event_cb);

#define STR_WIFI_SECOND_CHAN_NONE "HT20"
#define STR_WIFI_SECOND_CHAN_ABOVE "HT40_ABOVE"
#define STR_WIFI_SECOND_CHAN_BELOW "HT40_BELOW"
extern const char * const wifi_second_chan_names[];

int wifi_getmac (wifi_interface_t interface, lua_State *L);

#endif
