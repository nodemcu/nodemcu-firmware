#ifndef COAP_H
#define COAP_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "c_stdint.h"
#include "c_stddef.h"
#include "lualib.h"
#include "lauxlib.h"
#include "coap_define.h"

#define MAXOPT 16
#define MAX_MESSAGE_SIZE 1152
#define MAX_PAYLOAD_SIZE 1024
#define MAX_REQUEST_SIZE 576
#define MAX_REQ_SCRATCH_SIZE 60

#define COAP_RESPONSE_CLASS(C) (((C) >> 5) & 0xFF)

//http://tools.ietf.org/html/rfc7252#section-3
typedef struct
{
    uint8_t ver;                /* CoAP version number */
    uint8_t t;                  /* CoAP Message Type */
    uint8_t tkl;                /* Token length: indicates length of the Token field */
    uint8_t code;               /* CoAP status code. Can be request (0.xx), success reponse (2.xx), 
                                 * client error response (4.xx), or rever error response (5.xx) 
                                 * For possible values, see http://tools.ietf.org/html/rfc7252#section-12.1 */
    uint8_t id[2];
} coap_header_t;

typedef struct
{
    const uint8_t *p;
    size_t len;
} coap_buffer_t;

typedef struct
{
    uint8_t *p;
    size_t len;
} coap_rw_buffer_t;

typedef struct
{
    uint8_t num;                /* Option number. See http://tools.ietf.org/html/rfc7252#section-5.10 */
    coap_buffer_t buf;          /* Option value */
} coap_option_t;

typedef struct
{
    coap_header_t hdr;          /* Header of the packet */
    coap_buffer_t tok;          /* Token value, size as specified by hdr.tkl */
    uint8_t numopts;            /* Number of options */
    coap_option_t opts[MAXOPT]; /* Options of the packet. For possible entries see
                                 * http://tools.ietf.org/html/rfc7252#section-5.10 */
    coap_buffer_t payload;      /* Payload carried by the packet */
    coap_rw_buffer_t content;       // content->p = malloc(...) , and free it when done.
} coap_packet_t;

/////////////////////////////////////////

///////////////////////
/*
typedef struct coap_endpoint_t coap_endpoint_t;

typedef int (*coap_endpoint_func)(const coap_endpoint_t *ep, coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo);
#define MAX_SEGMENTS 3  // 2 = /foo/bar, 3 = /foo/bar/baz
#define MAX_SEGMENTS_SIZE   16
typedef struct
{
    int count;
    const char *elems[MAX_SEGMENTS];
} coap_endpoint_path_t;

typedef struct coap_luser_entry coap_luser_entry;

struct coap_luser_entry{
    // int ref;
    // char name[MAX_SEGMENTS_SIZE+1];         // +1 for string '\0'
    const char *name;
    coap_luser_entry *next;
    int content_type;
};
*/
//struct coap_endpoint_t{
//    coap_method_t method;               /* (i.e. POST, PUT or GET) */
//    coap_endpoint_func handler;         /* callback function which handles this 
//                                         * type of endpoint (and calls 
//                                         * coap_make_response() at some point) */
//    const coap_endpoint_path_t *path;   /* path towards a resource (i.e. foo/bar/) */ 
//    const char *core_attr;              /* the 'ct' attribute, as defined in RFC7252, section 7.2.1.:
//                                         * "The Content-Format code "ct" attribute 
//                                         * provides a hint about the 
//                                         * Content-Formats this resource returns." 
//                                         * (Section 12.3. lists possible ct values.) */
//	coap_luser_entry *user_entry;
//};

#include "coap_define.h"

///////////////////////
void coap_dumpPacket(coap_packet_t *pkt);
int coap_parse(coap_packet_t *pkt, const uint8_t *buf, size_t buflen);
int coap_buffer_to_string(char *strbuf, size_t strbuflen, const coap_buffer_t *buf);
const coap_option_t *coap_findOptions(const coap_packet_t *pkt, uint8_t num, uint8_t *count);
int coap_build(uint8_t *buf, size_t *buflen, const coap_packet_t *pkt);
void coap_dump(const uint8_t *buf, size_t buflen, bool bare);
//int coap_make_response(coap_rw_buffer_t *scratch, coap_packet_t *pkt, const uint8_t *content, size_t content_len, uint8_t msgid_hi, uint8_t msgid_lo, const coap_buffer_t* tok, coap_responsecode_t rspcode, coap_content_type_t content_type);
//int coap_handle_req(coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt);
void coap_option_nibble(uint32_t value, uint8_t *nibble);
void coap_setup(void);
void endpoint_setup(void);

int coap_buildOptionHeader(uint32_t optDelta, size_t length, uint8_t *buf, size_t buflen);
int check_token(coap_packet_t *pkt);

#include "uri.h"
int coap_make_request(coap_rw_buffer_t *scratch, unsigned short message_id, coap_packet_t *pkt, coap_msgtype_t t, coap_method_t m, coap_uri_t *uri, const uint8_t *payload, size_t payload_len);


#ifdef __cplusplus
}
#endif

#endif
