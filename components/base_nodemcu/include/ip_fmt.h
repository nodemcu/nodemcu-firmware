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

#ifndef _IP_FMT_H_
#define _IP_FMT_H_

#include <stdint.h>
#include "lwip/ip_addr.h"
#include "esp_netif_ip_addr.h"

// String buffer for a formatted MAC/BSSID
#define MAC_STR_SZ (6*2+5+1)
void macstr (char *out, const uint8_t *mac);

// String buffer for a formatted IPv4 or IPv6 address
#define IP_STR_SZ (8*4+7+1)
void ipstr (char *out, const ip_addr_t *ip);
void ip4str (char *out, const ip4_addr_t *ip);

void ipstr_esp (char *out, const esp_ip_addr_t *ip);
void ip4str_esp (char *out, const esp_ip4_addr_t *ip);

#ifdef CONFIG_LWIP_IPV6
void ip6str (char *out, const ip6_addr_t *ip);
void ip6str_esp (char *out, const esp_ip6_addr_t *ip);
#endif

#endif
