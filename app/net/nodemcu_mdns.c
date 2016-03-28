/**
 * lwip MDNS resolver file.
 *
 * Created on: Jul 29, 2010
 * Author: Daniel Toma
 *

 * ported from uIP resolv.c Copyright (c) 2002-2003, Adam Dunkels.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**

 * This file implements a MDNS host name and PUCK service registration.

 *-----------------------------------------------------------------------------
 * Includes
 *----------------------------------------------------------------------------*/
#include "lwip/opt.h"
#if LWIP_MDNS /* don't build if not configured for use in lwipopts.h */
#include "lwip/mdns.h"
#include "lwip/udp.h"
#include "lwip/mem.h"
#include "lwip/igmp.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"
#include "c_string.h"
#include "nodemcu_mdns.h"

#if 0
#define MDNS_DBG(...)  os_printf(...)
#else
#define MDNS_DBG(...)  do {} while (0)
#endif

#define MDNS_NAME_LENGTH     68 //68

static const char* service_name_with_suffix    = NULL;
#define DNS_SD_SERVICE       "_services._dns-sd._udp.local"
#define PUCK_SERVICE_LENGTH  30

#define PUCK_DATASHEET_SIZE   	96

#ifdef MEMLEAK_DEBUG
static const char mem_debug_file[] ICACHE_RODATA_ATTR = __FILE__;
#endif

/** DNS server IP address */
#ifndef DNS_MULTICAST_ADDRESS
#define DNS_MULTICAST_ADDRESS        ipaddr_addr("224.0.0.251") /* resolver1.opendns.com */
#endif

/** DNS server IP address */
#ifndef MDNS_LOCAL
#define MDNS_LOCAL                "local" /* resolver1.opendns.com */
#endif

/** DNS server port address */
#ifndef DNS_MDNS_PORT
#define DNS_MDNS_PORT           5353
#endif

/** DNS maximum number of retries when asking for a name, before "timeout". */
#ifndef DNS_MAX_RETRIES
#define DNS_MAX_RETRIES           4
#endif

/** DNS resource record max. TTL (one week as default) */
#ifndef DNS_MAX_TTL
#define DNS_MAX_TTL               604800
#endif

/* DNS protocol flags */
#define DNS_FLAG1_RESPONSE        0x84
#define DNS_FLAG1_OPCODE_STATUS   0x10
#define DNS_FLAG1_OPCODE_INVERSE  0x08
#define DNS_FLAG1_OPCODE_STANDARD 0x00
#define DNS_FLAG1_AUTHORATIVE     0x04
#define DNS_FLAG1_TRUNC           0x02
#define DNS_FLAG1_RD              0x01
#define DNS_FLAG2_RA              0x80
#define DNS_FLAG2_ERR_MASK        0x0f
#define DNS_FLAG2_ERR_NONE        0x00
#define DNS_FLAG2_ERR_NAME        0x03

/* DNS protocol states */
#define DNS_STATE_UNUSED          0
#define DNS_STATE_NEW             1
#define DNS_STATE_ASKING          2
#define DNS_STATE_DONE            3

/* MDNS registration type */
#define MDNS_HOSTNAME_REG         0
#define MDNS_SERVICE_REG          1

/* MDNS registration type */
#define MDNS_REG_ANSWER           1
#define MDNS_SD_ANSWER            2
#define MDNS_SERVICE_REG_ANSWER   3

/* MDNS registration time */
#define MDNS_HOST_TIME            120
#define MDNS_SERVICE_TIME         3600

/** MDNS name length with "." at the beginning and end of name*/
#ifndef MDNS_LENGTH_ADD
#define MDNS_LENGTH_ADD           2
#endif

#ifdef MDNS_MAX_NAME_LENGTH
#undef MDNS_MAX_NAME_LENGTH
#endif
#define MDNS_MAX_NAME_LENGTH      (256)

PACK_STRUCT_BEGIN
/** DNS message header */
struct mdns_hdr {
	PACK_STRUCT_FIELD(u16_t id);
	PACK_STRUCT_FIELD(u8_t flags1);
	PACK_STRUCT_FIELD(u8_t flags2);
	PACK_STRUCT_FIELD(u16_t numquestions);
	PACK_STRUCT_FIELD(u16_t numanswers);
	PACK_STRUCT_FIELD(u16_t numauthrr);
	PACK_STRUCT_FIELD(u16_t numextrarr);
}PACK_STRUCT_STRUCT;
PACK_STRUCT_END

#define SIZEOF_DNS_HDR 12

PACK_STRUCT_BEGIN
/** MDNS query message structure */
struct mdns_query {
	/* MDNS query record starts with either a domain name or a pointer
	 to a name already present somewhere in the packet. */PACK_STRUCT_FIELD(u16_t type);
	PACK_STRUCT_FIELD(u16_t class);
}PACK_STRUCT_STRUCT;
PACK_STRUCT_END

#define SIZEOF_DNS_QUERY 4

PACK_STRUCT_BEGIN
/** MDNS answer message structure */
struct mdns_answer {
	/* MDNS answer record starts with either a domain name or a pointer
	 to a name already present somewhere in the packet. */PACK_STRUCT_FIELD(u16_t type);
	PACK_STRUCT_FIELD(u16_t class);
	PACK_STRUCT_FIELD(u32_t ttl);
	PACK_STRUCT_FIELD(u16_t len);
}PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#define SIZEOF_DNS_ANSWER 10

PACK_STRUCT_BEGIN
/** MDNS answer message structure */
struct mdns_auth {
	PACK_STRUCT_FIELD(u32_t src);
}PACK_STRUCT_STRUCT;
PACK_STRUCT_END

#define SIZEOF_MDNS_AUTH 4
PACK_STRUCT_BEGIN
/** MDNS service registration message structure */
struct mdns_service {
	PACK_STRUCT_FIELD(u16_t prior);
	PACK_STRUCT_FIELD(u16_t weight);
	PACK_STRUCT_FIELD(u16_t port);
}PACK_STRUCT_STRUCT;
PACK_STRUCT_END

#define SIZEOF_MDNS_SERVICE 6

static uint16 PUCK_PORT ;
static os_timer_t mdns_timer;
/* forward declarations */
static void mdns_recv(void *s, struct udp_pcb *pcb, struct pbuf *p,
		struct ip_addr *addr, u16_t port);

/*-----------------------------------------------------------------------------
 * Globales
 *----------------------------------------------------------------------------*/

/* MDNS variables */
static const char *host_name;
static const char *host_description;
static const char *server_name;
//static char puck_datasheet[PUCK_DATASHEET_SIZE];
static struct udp_pcb *mdns_pcb = NULL;
static struct nodemcu_mdns_info * ms_info = NULL;
static struct ip_addr multicast_addr;
static struct ip_addr host_addr;
static uint8 register_flag = 0;
static uint8 mdns_flag = 0;
static u8_t *mdns_payload;
static void ICACHE_FLASH_ATTR
mdns_set_hostname(char *name);

/*
 *  Function to set the UDP pcb used to send the mDNS packages
 */
static void ICACHE_FLASH_ATTR
getPcb(struct udp_pcb *pcb) {
	mdns_pcb = pcb;
}

#if DNS_DOES_NAME_CHECK
/**
 * Compare the "dotted" name "query" with the encoded name "response"
 * to make sure an answer from the DNS server matches the current mdns_table
 * entry (otherwise, answers might arrive late for hostname not on the list
 * any more).
 *
 * @param query hostname (not encoded) from the mdns_table
 * @param response encoded hostname in the DNS response
 * @return 0: names equal; 1: names differ
 */
static u8_t ICACHE_FLASH_ATTR
mdns_compare_name(unsigned char *query, unsigned char *response) {
	unsigned char n;

	do {
		n = *response++;
		/** @see RFC 1035 - 4.1.4. Message compression */
		if ((n & 0xc0) == 0xc0) {
			/* Compressed name */
			break;
		} else {
			/* Not compressed name */
			while (n > 0) {
				if ((*query) != (*response)) {
					return 1;
				}
				++response;
				++query;
				--n;
			};
			++query;
		}
	} while (*response != 0);

	return 0;
}
#endif /* DNS_DOES_NAME_CHECK */
/**
 * Send a mDNS packet for the service type
 *
 * @param id transaction ID in the DNS query packet
 * @return ERR_OK if packet is sent; an err_t indicating the problem otherwise
 */
static err_t ICACHE_FLASH_ATTR
mdns_send_service_type(u16_t id, struct ip_addr *dst_addr, u16_t dst_port) {
	err_t err;
	struct mdns_hdr *hdr;
	struct mdns_answer ans;
	struct mdns_auth auth;
	struct mdns_service serv;
	struct pbuf *p ,*p_sta;
	char *query, *nptr;
	const char *pHostname;
	struct netif * sta_netif = NULL;
	struct netif * ap_netif = NULL;
	char tmpBuf[PUCK_DATASHEET_SIZE + PUCK_SERVICE_LENGTH];
	u8_t n;
	u16_t length = 0;
	/* if here, we have either a new query or a retry on a previous query to process */
	p = pbuf_alloc(PBUF_TRANSPORT,
			SIZEOF_DNS_HDR + MDNS_MAX_NAME_LENGTH * 2 + SIZEOF_DNS_QUERY, PBUF_RAM);
	if (p != NULL) {
		LWIP_ASSERT("pbuf must be in one piece", p->next == NULL);
		/* fill dns header */
		hdr = (struct mdns_hdr*) p->payload;
		os_memset(hdr, 0, SIZEOF_DNS_HDR);
		hdr->id = htons(id);
		hdr->flags1 = DNS_FLAG1_RESPONSE;

		pHostname = DNS_SD_SERVICE;
		hdr->numanswers = htons(1);
		query = (char*) hdr + SIZEOF_DNS_HDR;
		--pHostname;
		/* convert hostname into suitable query format. */
		do {
			++pHostname;
			nptr = query;
			++query;
			for (n = 0; *pHostname != '.' && *pHostname != 0; ++pHostname) {
				*query = *pHostname;
				++query;
				++n;
			}
			*nptr = n;
		} while (*pHostname != 0);
		*query++ = '\0';
		/* fill dns query */


		ans.type = htons(DNS_RRTYPE_PTR);
		ans.class = htons(DNS_RRCLASS_IN);
		ans.ttl = htonl(3600);
		ans.len = htons(os_strlen(service_name_with_suffix) + 1 +1 );
		length = 0;

		MEMCPY( query, &ans, SIZEOF_DNS_ANSWER);

		/* resize the query */
		query = query + SIZEOF_DNS_ANSWER;
		pHostname = service_name_with_suffix;
		--pHostname;

		/* convert hostname into suitable query format. */
		do {
			++pHostname;
			nptr = query;
			++query;
			for (n = 0; *pHostname != '.' && *pHostname != 0; ++pHostname) {
				*query = *pHostname;
				++query;
				++n;
			}
			*nptr = n;
		} while (*pHostname != 0);
		*query++ = '\0';

		/* resize pbuf to the exact dns query */
		pbuf_realloc(p, (query + length) - ((char*) (p->payload)));

		/* send dns packet */
		if (dst_addr) {
		  err = udp_sendto(mdns_pcb, p, dst_addr, dst_port);
		} else {
		  /*add by tzx for AP + STA MDNS begin------*/
		  sta_netif = (struct netif *)eagle_lwip_getif(0x00);
		  ap_netif =  (struct netif *)eagle_lwip_getif(0x01);
		  if(wifi_get_opmode() == 0x03 && wifi_get_broadcast_if() == 0x03 &&\
				  sta_netif != NULL && ap_netif != NULL) {
			  if(netif_is_up(sta_netif) && netif_is_up(ap_netif)) {

				  p_sta = pbuf_alloc(PBUF_TRANSPORT,
							  SIZEOF_DNS_HDR + MDNS_MAX_NAME_LENGTH * 2 + SIZEOF_DNS_QUERY, PBUF_RAM);
			    if (pbuf_copy (p_sta,p) != ERR_OK) {
				    MDNS_DBG("mdns_answer copying to new pbuf failed\n");
				    return -1;
			    }
			    netif_set_default(sta_netif);
			    err = udp_sendto(mdns_pcb, p_sta, &multicast_addr, DNS_MDNS_PORT);
			    pbuf_free(p_sta);
			    netif_set_default(ap_netif);
			  }
		  }
		  /*add by tzx for AP + STA MDNS end------*/
		  err = udp_sendto(mdns_pcb, p, &multicast_addr, DNS_MDNS_PORT);
		}
		/* free pbuf */
		pbuf_free(p);
	} else {
		err = ERR_MEM;
	}

	return err;
}

/**
 * Send a mDNS service answer packet.
 *
 * @param name service name to query
 * @param id transaction ID in the DNS query packet
 * @return ERR_OK if packet is sent; an err_t indicating the problem otherwise
 */
static err_t ICACHE_FLASH_ATTR
mdns_send_service(struct nodemcu_mdns_info *info, u16_t id, struct ip_addr *dst_addr, u16_t dst_port) {
	err_t err;
	struct mdns_hdr *hdr;
	struct mdns_answer ans;
	struct mdns_service serv;
	struct mdns_auth auth;
	struct pbuf *p ,*p_sta;
	char *query, *nptr;
	char *query_end;
	const char *pHostname;
	const char *name = info->host_name;
	u8_t n;
	u8_t i = 0;
	u16_t length = 0;
	u8_t addr1 = 12, addr2 = 12;
	struct netif * sta_netif = NULL;
	struct netif * ap_netif = NULL;
	char tmpBuf[PUCK_DATASHEET_SIZE + PUCK_SERVICE_LENGTH];
	u16_t dns_class = dst_addr ? DNS_RRCLASS_IN : DNS_RRCLASS_FLUSH_IN;
	/* if here, we have either a new query or a retry on a previous query to process */
	p = pbuf_alloc(PBUF_TRANSPORT,
			SIZEOF_DNS_HDR + MDNS_MAX_NAME_LENGTH * 2 + SIZEOF_DNS_QUERY, PBUF_RAM);
	if (p != NULL) {
		LWIP_ASSERT("pbuf must be in one piece", p->next == NULL);
		/* fill dns header */
		hdr = (struct mdns_hdr*) p->payload;
		os_memset(hdr, 0, SIZEOF_DNS_HDR);
		hdr->id = htons(id);
		hdr->flags1 = DNS_FLAG1_RESPONSE;
		hdr->numanswers = htons(4);
		query = (char*) hdr + SIZEOF_DNS_HDR;
		query_end = (char *) p->payload + p->tot_len;
		c_strlcpy(tmpBuf, service_name_with_suffix, sizeof(tmpBuf));

		pHostname = tmpBuf;
		--pHostname;

		/* convert hostname into suitable query format. */
		do {
			++pHostname;
			nptr = query;
			++query;
			++addr1;
			++addr2;
			for (n = 0; *pHostname != '.' && *pHostname != 0; ++pHostname) {
				*query = *pHostname;
				++query;
				++addr1;
				++addr2;
				++n;
			}
			*nptr = n;
		} while (*pHostname != 0);
		*query++ = '\0';
		length = sizeof(MDNS_LOCAL);
		addr1 -= length;
		length = os_strlen(service_name_with_suffix) + 1;
		addr2 -= length;

		ans.type = htons(DNS_RRTYPE_PTR);
		ans.class = htons(DNS_RRCLASS_IN);
		ans.ttl = htonl(300);
		length = os_strlen(host_description) + MDNS_LENGTH_ADD + 1;
		ans.len = htons(length);
		length = 0;

		MEMCPY( query, &ans, SIZEOF_DNS_ANSWER);
		/* resize the query */
		query = query + SIZEOF_DNS_ANSWER;

		int name_offset = query - (const char *) hdr;

		pHostname = host_description;
		--pHostname;
		/* convert hostname into suitable query format. */
		do {
			++pHostname;
			nptr = query;
			++query;
			for (n = 0; *pHostname != '.' && *pHostname != 0; ++pHostname) {
				*query = *pHostname;
				++query;
				++n;
			}
			*nptr = n;
		} while (*pHostname != 0);
		*query++ = DNS_OFFSET_FLAG;
		*query++ = DNS_DEFAULT_OFFSET;

		*query++ = 0xc0 + (name_offset >> 8);
		*query++ = name_offset & 0xff;

		/* fill the answer */
		ans.type = htons(DNS_RRTYPE_TXT);
		ans.class = htons(dns_class);
		ans.ttl = htonl(300);
//		length = os_strlen(TXT_DATA) + MDNS_LENGTH_ADD + 1;
		const char *attributes[12];
		int attr_count = 0;
		for(i = 0; i < 10 && (info->txt_data[i] != NULL); i++) {
		  length += os_strlen(info->txt_data[i]);
		  length++;
		  attributes[attr_count++] = info->txt_data[i];
		}
		//MDNS_DBG("Found %d user attributes\n", i);
		static const char *defaults[] = { "platform=nodemcu", NULL };
		for(i = 0; defaults[i] != NULL; i++) {
		  // See if this is a duplicate
		  int j;
		  int len = strchr(defaults[i], '=') + 1 - defaults[i];
		  for (j = 0; j < attr_count; j++) {
		    if (strncmp(attributes[j], defaults[i], len) == 0) {
		      break;
		    }
		  }
		  if (j == attr_count) {
		    length += os_strlen(defaults[i]);
		    length++;
		    attributes[attr_count++] = defaults[i];
		  }
		}
		//MDNS_DBG("Found %d total attributes\n", attr_count);

		ans.len = htons(length);
		MEMCPY( query, &ans, SIZEOF_DNS_ANSWER);
		query = query + SIZEOF_DNS_ANSWER;
		// Check enough space in the packet
	        const char *end_of_packet = query + length + 2 + SIZEOF_DNS_ANSWER + SIZEOF_MDNS_SERVICE +
		    os_strlen(host_name) + 7 + 1 + 2 + SIZEOF_DNS_ANSWER + SIZEOF_MDNS_AUTH;

	        if (query_end <= end_of_packet) {
		  MDNS_DBG("Too much data to send\n");
		  pbuf_free(p);
		  return ERR_MEM;
		}
		//MDNS_DBG("Query=%x, query_end=%x, end_ofpacket=%x, length=%x\n", query, query_end, end_of_packet, length);

		i = 0;
		while(attributes[i] != NULL && i < attr_count) {
			pHostname = attributes[i];
			--pHostname;
			/* convert hostname into suitable query format. */
			do {
				++pHostname;
				nptr = query;
				++query;
				for (n = 0;  *pHostname != 0; ++pHostname) {
					*query = *pHostname;
					++query;
					++n;
				}
				*nptr = n;
			} while (*pHostname != 0);
			i++;
		}
//		*query++ = '\0';
		// Increment by length
		*query++ = 0xc0 + (name_offset >> 8);
		*query++ = name_offset & 0xff;

		// Increment by 2

		ans.type = htons(DNS_RRTYPE_SRV);
		ans.class = htons(dns_class);
		ans.ttl = htonl(300);
		c_strlcpy(tmpBuf,host_name, sizeof(tmpBuf));
		c_strlcat(tmpBuf, ".", sizeof(tmpBuf));
		c_strlcat(tmpBuf, MDNS_LOCAL, sizeof(tmpBuf));
		length = os_strlen(tmpBuf) + MDNS_LENGTH_ADD;
		ans.len = htons(SIZEOF_MDNS_SERVICE + length);
		length = 0;
		MEMCPY( query, &ans, SIZEOF_DNS_ANSWER);

		/* resize the query */
		query = query + SIZEOF_DNS_ANSWER;

		serv.prior = htons(0);
		serv.weight = htons(0);
		serv.port = htons(PUCK_PORT);
		MEMCPY( query, &serv, SIZEOF_MDNS_SERVICE);
		/* resize the query */
		query = query + SIZEOF_MDNS_SERVICE;

		int hostname_offset = query - (const char *) hdr;

		pHostname = tmpBuf;
		--pHostname;
		do {
			++pHostname;
			nptr = query;
			++query;
			for (n = 0; *pHostname != '.' && *pHostname != 0; ++pHostname) {
				*query = *pHostname;
				++query;
				++n;
			}
			*nptr = n;
		} while (*pHostname != 0);
		*query++ = '\0';

		// increment by strlen(service_name) + 1 + 7 + sizeof_dns_answer + sizeof_mdns_service
		
		*query++ = 0xc0 + (hostname_offset >> 8);
		*query++ = hostname_offset & 0xff;

		ans.type = htons(DNS_RRTYPE_A);
		ans.class = htons(dns_class);
		ans.ttl = htonl(300);
		ans.len = htons(DNS_IP_ADDR_LEN);

		MEMCPY( query, &ans, SIZEOF_DNS_ANSWER);

		/* resize the query */
		query = query + SIZEOF_DNS_ANSWER;
		
		// increment by strlen(service_name) + 1 + 7 + sizeof_dns_answer 
		

		/* fill the payload of the mDNS message */
		/* set the local IP address */
		auth.src = host_addr.addr; //ipAddr;
		MEMCPY( query, &auth, SIZEOF_MDNS_AUTH);
		/* resize the query */
		query = query + SIZEOF_MDNS_AUTH;

		//MDNS_DBG("Final ptr=%x\n", query);

		// increment by sizeof_mdns_auth

		/* set the name of the authority field.
		 * The same name as the Query using the offset address*/

		/* resize pbuf to the exact dns query */
		pbuf_realloc(p, (query) - ((char*) (p->payload)));
		/* send dns packet */
		if (dst_addr) {
		  err = udp_sendto(mdns_pcb, p, dst_addr, dst_port);
		} else {
		  sta_netif = (struct netif *)eagle_lwip_getif(0x00);
		  ap_netif =  (struct netif *)eagle_lwip_getif(0x01);
		  if(wifi_get_opmode() == 0x03 && wifi_get_broadcast_if() == 0x03 &&\
				  sta_netif != NULL && ap_netif != NULL) {
		    if(netif_is_up(sta_netif) && netif_is_up(ap_netif)) {

		      p_sta = pbuf_alloc(PBUF_TRANSPORT,
					SIZEOF_DNS_HDR + MDNS_MAX_NAME_LENGTH * 2 + SIZEOF_DNS_QUERY, PBUF_RAM);
		      if (pbuf_copy (p_sta,p) != ERR_OK) {
			      MDNS_DBG("mdns_send_service copying to new pbuf failed\n");
			      return -1;
		      }
		      netif_set_default(sta_netif);
		      err = udp_sendto(mdns_pcb, p_sta, &multicast_addr, DNS_MDNS_PORT);
		      pbuf_free(p_sta);
		      netif_set_default(ap_netif);
		    }
		  }
		  err = udp_sendto(mdns_pcb, p, &multicast_addr, DNS_MDNS_PORT);
		}

		/* free pbuf */
		pbuf_free(p);
	} else {
		MDNS_DBG("ERR_MEM \n");
		err = ERR_MEM;
	}

	return err;
}

/**
 * Receive input function for DNS response packets arriving for the dns UDP pcb.
 *
 * @params see udp.h
 */
static void ICACHE_FLASH_ATTR
mdns_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr,
		u16_t port) {
	u16_t i;
	struct mdns_hdr *hdr;
	u8_t nquestions;
	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(pcb);
	struct nodemcu_mdns_info *info = (struct nodemcu_mdns_info *)arg;
	/* is the dns message too big ? */
	if (p->tot_len > DNS_MSG_SIZE) {
		LWIP_DEBUGF(DNS_DEBUG, ("dns_recv: pbuf too big\n"));
		/* free pbuf and return */
		goto memerr1;
	}

	/* is the dns message big enough ? */
	if (p->tot_len < (SIZEOF_DNS_HDR + SIZEOF_DNS_QUERY + SIZEOF_DNS_ANSWER)) {
		LWIP_DEBUGF(DNS_DEBUG, ("dns_recv: pbuf too small\n"));
		/* free pbuf and return */
		goto memerr1;
	}
	/* copy dns payload inside static buffer for processing */
	if (pbuf_copy_partial(p, mdns_payload, p->tot_len, 0) == p->tot_len) {
		/* The ID in the DNS header should be our entry into the name table. */
		hdr = (struct mdns_hdr*) mdns_payload;

		i = htons(hdr->id);
//		if (i < DNS_TABLE_SIZE) {

			nquestions = htons(hdr->numquestions);
			//nanswers   = htons(hdr->numanswers);
			/* if we have a question send an answer if necessary */
			if (nquestions > 0) {
				/* MDNS_DS_DOES_NAME_CHECK */
				/* Check if the name in the "question" part match with the name of the MDNS DS service. */
				if (mdns_compare_name((unsigned char *) DNS_SD_SERVICE,
						(unsigned char *) mdns_payload + SIZEOF_DNS_HDR) == 0) {
					/* respond with the puck service*/
					mdns_send_service_type(i, addr, port);
				} else if (mdns_compare_name((unsigned char *) service_name_with_suffix,
						(unsigned char *) mdns_payload + SIZEOF_DNS_HDR) == 0) {
					/* respond with the puck service*/
					mdns_send_service(info, i, addr, port);
				} else {
				  
				  char tmpBuf[PUCK_DATASHEET_SIZE + PUCK_SERVICE_LENGTH];
				  c_strlcpy(tmpBuf,host_name, sizeof(tmpBuf));
				  c_strlcat(tmpBuf, ".", sizeof(tmpBuf));
				  c_strlcat(tmpBuf, MDNS_LOCAL, sizeof(tmpBuf));

				  if (mdns_compare_name((unsigned char *) tmpBuf,
						(unsigned char *) mdns_payload + SIZEOF_DNS_HDR) == 0) {
					/* respond with the puck service*/
					mdns_send_service(info, i, addr, port);
				  } else {
				    c_strlcpy(tmpBuf,host_description, sizeof(tmpBuf));
				    c_strlcat(tmpBuf, ".", sizeof(tmpBuf));
				    c_strlcat(tmpBuf, service_name_with_suffix, sizeof(tmpBuf));
				    if (mdns_compare_name((unsigned char *) tmpBuf,
						(unsigned char *) mdns_payload + SIZEOF_DNS_HDR) == 0) {
					/* respond with the puck service*/
					mdns_send_service(info, i, addr, port);
				    } else {
					goto memerr2;
				    }
				  }
				}
			}
//		}
	}
	goto memerr2;
	memerr2:
	os_memset(mdns_payload , 0 ,DNS_MSG_SIZE);
	memerr1:
	/* free pbuf */
	pbuf_free(p);
	return;
}

/**
 * close the UDP pcb .
 */
void ICACHE_FLASH_ATTR
nodemcu_mdns_close(void)
{
	os_timer_disarm(&mdns_timer);

	if (mdns_pcb != NULL) {
		udp_remove(mdns_pcb);
	}
	if (mdns_payload) {
	  os_free(mdns_payload);
	}
	mdns_payload = NULL;
	mdns_pcb = NULL;
	ms_info = NULL;
}

static void ICACHE_FLASH_ATTR
mdns_set_host_description(const char *name)
{
	host_description = name;
}

static void ICACHE_FLASH_ATTR
mdns_enable(void)
{
	if(mdns_flag == 0) {
		udp_recv(mdns_pcb, mdns_recv, NULL);
	}
}

static void ICACHE_FLASH_ATTR
mdns_disable(void)
{
	if (mdns_flag == 1) {
		udp_recv(mdns_pcb, NULL, NULL);
	}
}

static void ICACHE_FLASH_ATTR
mdns_set_hostname(char *name) {
	if (name == NULL) {
	  name = "nodemcu";
	}
	host_name = name;
}

static void ICACHE_FLASH_ATTR
mdns_set_servername(const char *name) {
	if (name == NULL) {
		name = "nodemcu";
	}
	char tmpBuf[128];
	os_sprintf(tmpBuf, "_%s._tcp.local", name);
	if (server_name) {
	  os_free(server_name);
	}
	server_name = c_strdup(tmpBuf);
	service_name_with_suffix = server_name;
}

static void ICACHE_FLASH_ATTR
mdns_server_unregister(void) {
	struct ip_addr ap_host_addr;
	struct ip_info ipconfig;
	if(register_flag == 1){
		if (igmp_leavegroup(&host_addr, &multicast_addr) != ERR_OK) {
			MDNS_DBG("sta udp_leave_multigrup failed!\n");
			return;
		};
		if(wifi_get_opmode() == 0x03 || wifi_get_opmode() == 0x02) {
		   wifi_get_ip_info(SOFTAP_IF, &ipconfig);
		   ap_host_addr.addr = ipconfig.ip.addr;
		   if (igmp_leavegroup(&ap_host_addr, &multicast_addr) != ERR_OK) {
				MDNS_DBG("ap udp_join_multigrup failed!\n");
				return;
			};
		}
		os_timer_disarm(&mdns_timer);
		register_flag = 0;
	}
}

static void ICACHE_FLASH_ATTR
mdns_server_register(void) {

	if (register_flag == 1) {
		MDNS_DBG("mdns server is already registered !\n");
		return;
	} else if (igmp_joingroup(&host_addr, &multicast_addr) != ERR_OK) {
		MDNS_DBG("udp_join_multigrup failed!\n");
		return;
	};
	register_flag = 1;
}

static u8_t reg_counter;

static void
mdns_reg_handler_restart(void) {
  reg_counter = 99;
}

static void ICACHE_FLASH_ATTR
mdns_reg(struct nodemcu_mdns_info *info) {
  mdns_send_service(info,0,0,0);
  if (reg_counter++ > 10) {
    mdns_send_service_type(0,0,0);
    reg_counter = 0;
  }
}

/**
 * Initialize the resolver: set up the UDP pcb and configure the default server
 * (NEW IP).
 */
void ICACHE_FLASH_ATTR
nodemcu_mdns_init(struct nodemcu_mdns_info *info) {
	/* initialize default DNS server address */
	multicast_addr.addr = DNS_MULTICAST_ADDRESS;
	struct ip_addr ap_host_addr;
	struct ip_info ipconfig;
	ms_info = info;		// Save the passed block. We need all the data forever

	if (mdns_payload) {
	  os_free(mdns_payload);
	}
	mdns_payload = (u8_t *) os_malloc(DNS_MSG_SIZE);
	if (!mdns_payload) {
	  MDNS_DBG("Alloc fail\n");
	  return;
	}

	if (ms_info->ipAddr == 0) {
		MDNS_DBG("mdns ip error!\n ");
		return;
	}
	host_addr.addr = ms_info->ipAddr ;
	LWIP_DEBUGF(DNS_DEBUG, ("dns_init: initializing\n"));

	mdns_set_hostname(ms_info->host_name);
	mdns_set_servername(ms_info->service_name);
	mdns_set_host_description(ms_info->host_desc);

	// get the host name as instrumentName_serialNumber for MDNS
	// set the name of the service, the same as host name
	MDNS_DBG("host_name = %s\n", host_name);
	MDNS_DBG("server_name = %s\n", service_name_with_suffix);
	if (ms_info->server_port == 0)
	{
		PUCK_PORT = 80;
	} else {
		PUCK_PORT = ms_info->server_port;
	}

	/* initialize mDNS */
	mdns_pcb = udp_new();

	if (mdns_pcb != NULL) {
		/* join to the multicast address 224.0.0.251 */
		if(wifi_get_opmode() == 0x03 || wifi_get_opmode() == 0x01) {
			if (igmp_joingroup(&host_addr, &multicast_addr) != ERR_OK) {
				MDNS_DBG("sta udp_join_multigrup failed!\n");
				return;
			};
		}
		if(wifi_get_opmode() == 0x03 || wifi_get_opmode() == 0x02) {
		   wifi_get_ip_info(SOFTAP_IF, &ipconfig);
		   ap_host_addr.addr = ipconfig.ip.addr;
		   if (igmp_joingroup(&ap_host_addr, &multicast_addr) != ERR_OK) {
				MDNS_DBG("ap udp_join_multigrup failed!\n");
				return;
			};
		}
		register_flag = 1;
		/* join to any IP address at the port 5353 */
		if (udp_bind(mdns_pcb, IP_ADDR_ANY, DNS_MDNS_PORT) != ERR_OK) {
			MDNS_DBG("udp_bind failed!\n");
			return;
		};

		/*loopback function for the multicast(224.0.0.251) messages received at port 5353*/
//		mdns_enable();
		udp_recv(mdns_pcb, mdns_recv, ms_info);
		mdns_flag = 1;
		/*
		 * Register the name of the instrument
		 */

		//MDNS_DBG("About to start timer\n");
		os_timer_disarm(&mdns_timer);
		os_timer_setfn(&mdns_timer, (os_timer_func_t *)mdns_reg,ms_info);
		os_timer_arm(&mdns_timer, 1000 * 120, 1);
		/* kick off the first one right away */
		mdns_reg_handler_restart();
		mdns_reg(ms_info);
	}
}

#endif /* LWIP_MDNS */
