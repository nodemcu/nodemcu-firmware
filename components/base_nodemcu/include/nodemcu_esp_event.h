/*
 * Copyright 2016-2019 Dius Computing Pty Ltd. All rights reserved.
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

#ifndef _NODEMCU_ESP_EVENT_H_
#define _NODEMCU_ESP_EVENT_H_

#include "esp_event.h"
#include "module.h"
#include <assert.h>
#include <stdalign.h>

/**
 * Similarly to how a NodeMCU module is registered, a module can register
 * to receive copies of ESP system events (e.g. WiFi stack notifications).
 * These notifications will come in the form of callbacks within the main
 * Lua thread context, making it possible to directly relay information
 * through to Lua callbacks/globals.
 *
 * Registering is as simple as including this header file, then adding a
 * line for each event the module wishes to be notified about, e.g.:
 *
 *  NODEMCU_ESP_EVENT(IP_EVENT, IP_EVENT_STA_GOT_IP, do_stuff_when_got_ip);
 *  NODEMCU_ESP_EVENT(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, do_stuff_when_discon);
 *
 * These registrations are done at link-time, and consume no additional RAM.
 * The event IDs are located in esp_event.h in the IDF.
 */

// Event callback prototype
typedef void (*nodemcu_esp_event_cb) (esp_event_base_t event_base, int32_t event_id, const void *event_data);

// Internal definitions
typedef struct {
  const esp_event_base_t *event_base_ptr;
  int32_t                 event_id;
  nodemcu_esp_event_cb    callback;
} nodemcu_esp_event_reg_t;

extern nodemcu_esp_event_reg_t _esp_event_cb_table_start;
extern nodemcu_esp_event_reg_t _esp_event_cb_table_end;

#define NODEMCU_ESP_EVENT(evbase, evcode, func) \
  static const LOCK_IN_SECTION(esp_event_cb_table) \
    nodemcu_esp_event_reg_t MODULE_PASTE_(func,evbase##evcode) = { \
      .event_base_ptr = &evbase, \
      .event_id = evcode, \
      .callback = func };

_Static_assert(_Alignof(nodemcu_esp_event_reg_t) == 4, "Unexpected alignment of event registration - update linker script snippets to match!");

#endif
