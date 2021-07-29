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
#include "ip_fmt.h"
#include "esp_netif.h"
#include <stdio.h>

void macstr (char *str, const uint8_t *mac)
{
  sprintf (str, "%02x:%02x:%02x:%02x:%02x:%02x",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}



void ip4str (char *out, const ip4_addr_t *ip)
{
  ip4addr_ntoa_r(ip, out, IP_STR_SZ);
}



void ip4str_esp (char *out, const esp_ip4_addr_t *ip)
{
  esp_ip4addr_ntoa(ip, out, IP_STR_SZ);
}


#ifdef CONFIG_LWIP_IPV6

void ipstr (char *out, const ip_addr_t *ip)
{
  if (ip->type == IPADDR_TYPE_V4)
    ip4str (out, &ip->u_addr.ip4);
  else if (ip->type == IPADDR_TYPE_V6)
    ip6str (out, &ip->u_addr.ip6);
}

void ip6str (char *out, const ip6_addr_t *ip)
{
  ip6addr_ntoa_r(ip, out, IP_STR_SZ);
}

void ipstr_esp (char *out, const esp_ip_addr_t *ip)
{
  if (ip->type == ESP_IPADDR_TYPE_V4)
    ip4str_esp (out, &ip->u_addr.ip4);
  else if (ip->type == ESP_IPADDR_TYPE_V6)
    ip6str_esp (out, &ip->u_addr.ip6);
}

void ip6str_esp (char *out, const esp_ip6_addr_t *ip)
{
  ip6addr_ntoa_r((ip6_addr_t *)ip, out, IP_STR_SZ);
}

#else

void ipstr (char *out, const ip_addr_t *ip)
{
  ip4str(out, ip);
}

void ipstr_esp(char *out, const esp_ip_addr_t *ip)
{
  ip4str_esp(out, &ip->u_addr.ip4);
}

#endif
