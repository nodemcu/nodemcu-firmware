#include "user_config.h"
#include "c_stdio.h"
#include "c_string.h"
#include "coap.h"
#include "uri.h"

extern void endpoint_setup(void);
extern const coap_endpoint_t endpoints[];

#ifdef COAP_DEBUG
void coap_dumpHeader(coap_header_t *hdr)
{
    c_printf("Header:\n");
    c_printf("  ver  0x%02X\n", hdr->ver);
    c_printf("  t    0x%02X\n", hdr->ver);
    c_printf("  tkl  0x%02X\n", hdr->tkl);
    c_printf("  code 0x%02X\n", hdr->code);
    c_printf("  id   0x%02X%02X\n", hdr->id[0], hdr->id[1]);
}

void coap_dump(const uint8_t *buf, size_t buflen, bool bare)
{
    if (bare)
    {
        while(buflen--)
            c_printf("%02X%s", *buf++, (buflen > 0) ? " " : "");
    }
    else
    {
        c_printf("Dump: ");
        while(buflen--)
            c_printf("%02X%s", *buf++, (buflen > 0) ? " " : "");
        c_printf("\n");
    }
}
#endif

int coap_parseHeader(coap_header_t *hdr, const uint8_t *buf, size_t buflen)
{
    if (buflen < 4)
        return COAP_ERR_HEADER_TOO_SHORT;
    hdr->ver = (buf[0] & 0xC0) >> 6;
    if (hdr->ver != 1)
        return COAP_ERR_VERSION_NOT_1;
    hdr->t = (buf[0] & 0x30) >> 4;
    hdr->tkl = buf[0] & 0x0F;
    hdr->code = buf[1];
    hdr->id[0] = buf[2];
    hdr->id[1] = buf[3];
    return 0;
}

int coap_buildHeader(const coap_header_t *hdr, uint8_t *buf, size_t buflen)
{
    // build header
    if (buflen < 4)
        return COAP_ERR_BUFFER_TOO_SMALL;

    buf[0] = (hdr->ver & 0x03) << 6;
    buf[0] |= (hdr->t & 0x03) << 4;
    buf[0] |= (hdr->tkl & 0x0F);
    buf[1] = hdr->code;
    buf[2] = hdr->id[0];
    buf[3] = hdr->id[1];
    return 4;
}

int coap_parseToken(coap_buffer_t *tokbuf, const coap_header_t *hdr, const uint8_t *buf, size_t buflen)
{
    if (hdr->tkl == 0)
    {
        tokbuf->p = NULL;
        tokbuf->len = 0;
        return 0;
    }
    else
    if (hdr->tkl <= 8)
    {
        if (4U + hdr->tkl > buflen)
            return COAP_ERR_TOKEN_TOO_SHORT;   // tok bigger than packet
        tokbuf->p = buf+4;  // past header
        tokbuf->len = hdr->tkl;
        return 0;
    }
    else
    {
        // invalid size
        return COAP_ERR_TOKEN_TOO_SHORT;
    }
}

int coap_buildToken(const coap_buffer_t *tokbuf, const coap_header_t *hdr, uint8_t *buf, size_t buflen)
{
    // inject token
    uint8_t *p;
    if (buflen < (4U + hdr->tkl))
        return COAP_ERR_BUFFER_TOO_SMALL;
    p = buf + 4;
    if ((hdr->tkl > 0) && (hdr->tkl != tokbuf->len))
        return COAP_ERR_UNSUPPORTED;

    if (hdr->tkl > 0)
        c_memcpy(p, tokbuf->p, hdr->tkl);

    // http://tools.ietf.org/html/rfc7252#section-3.1
    // inject options
    return hdr->tkl;
}

// advances p
int coap_parseOption(coap_option_t *option, uint16_t *running_delta, const uint8_t **buf, size_t buflen)
{
    const uint8_t *p = *buf;
    uint8_t headlen = 1;
    uint16_t len, delta;

    if (buflen < headlen) // too small
        return COAP_ERR_OPTION_TOO_SHORT_FOR_HEADER;

    delta = (p[0] & 0xF0) >> 4;
    len = p[0] & 0x0F;

    // These are untested and may be buggy
    if (delta == 13)
    {
        headlen++;
        if (buflen < headlen)
            return COAP_ERR_OPTION_TOO_SHORT_FOR_HEADER;
        delta = p[1] + 13;
        p++;
    }
    else
    if (delta == 14)
    {
        headlen += 2;
        if (buflen < headlen)
            return COAP_ERR_OPTION_TOO_SHORT_FOR_HEADER;
        delta = ((p[1] << 8) | p[2]) + 269;
        p+=2;
    }
    else
    if (delta == 15)
        return COAP_ERR_OPTION_DELTA_INVALID;

    if (len == 13)
    {
        headlen++;
        if (buflen < headlen)
            return COAP_ERR_OPTION_TOO_SHORT_FOR_HEADER;
        len = p[1] + 13;
        p++;
    }
    else
    if (len == 14)
    {
        headlen += 2;
        if (buflen < headlen)
            return COAP_ERR_OPTION_TOO_SHORT_FOR_HEADER;
        len = ((p[1] << 8) | p[2]) + 269;
        p+=2;
    }
    else
    if (len == 15)
        return COAP_ERR_OPTION_LEN_INVALID;

    if ((p + 1 + len) > (*buf + buflen))
        return COAP_ERR_OPTION_TOO_BIG;

    //printf("option num=%d\n", delta + *running_delta);
    option->num = delta + *running_delta;
    option->buf.p = p+1;
    option->buf.len = len;
    //coap_dump(p+1, len, false);

    // advance buf
    *buf = p + 1 + len;
    *running_delta += delta;

    return 0;
}

// http://tools.ietf.org/html/rfc7252#section-3.1
int coap_parseOptionsAndPayload(coap_option_t *options, uint8_t *numOptions, coap_buffer_t *payload, const coap_header_t *hdr, const uint8_t *buf, size_t buflen)
{
    size_t optionIndex = 0;
    uint16_t delta = 0;
    const uint8_t *p = buf + 4 + hdr->tkl;
    const uint8_t *end = buf + buflen;
    int rc;
    if (p > end)
        return COAP_ERR_OPTION_OVERRUNS_PACKET;   // out of bounds

    //coap_dump(p, end - p);

    // 0xFF is payload marker
    while((optionIndex < *numOptions) && (p < end) && (*p != 0xFF))
    {
        if (0 != (rc = coap_parseOption(&options[optionIndex], &delta, &p, end-p)))
            return rc;
        optionIndex++;
    }
    *numOptions = optionIndex;

    if (p+1 < end && *p == 0xFF)  // payload marker
    {
        payload->p = p+1;
        payload->len = end-(p+1);
    }
    else
    {
        payload->p = NULL;
        payload->len = 0;
    }

    return 0;
}

int coap_buildOptionHeader(uint32_t optDelta, size_t length, uint8_t *buf, size_t buflen)
{
    int n = 0;
    uint8_t *p = buf;
    uint8_t len, delta = 0;

    if (buflen < 5)
        return COAP_ERR_BUFFER_TOO_SMALL;

    coap_option_nibble(optDelta, &delta);
    coap_option_nibble(length, &len);

    *p++ = (0xFF & (delta << 4 | len));
    n++;
    if (delta == 13)
    {
        *p++ = (optDelta - 13);
        n++;
    }
    else
    if (delta == 14)
    {
        *p++ = ((optDelta-269) >> 8);
        *p++ = (0xFF & (optDelta-269));
        n+=2;
    }
    if (len == 13)
    {
        *p++ = (length - 13);
        n++;
    }
    else
    if (len == 14)
    {
        *p++ = (length >> 8);
        *p++ = (0xFF & (length-269));
        n+=2;
    }
    return n;
}

#ifdef COAP_DEBUG
void coap_dumpOptions(coap_option_t *opts, size_t numopt)
{
    size_t i;
    c_printf(" Options:\n");
    for (i=0;i<numopt;i++)
    {
        c_printf("  0x%02X [ ", opts[i].num);
        coap_dump(opts[i].buf.p, opts[i].buf.len, true);
        c_printf(" ]\n");
    }
}

void coap_dumpPacket(coap_packet_t *pkt)
{
    coap_dumpHeader(&pkt->hdr);
    coap_dumpOptions(pkt->opts, pkt->numopts);
    c_printf("Payload: ");
    coap_dump(pkt->payload.p, pkt->payload.len, true);
    c_printf("\n");
}
#endif

int coap_parse(coap_packet_t *pkt, const uint8_t *buf, size_t buflen)
{
    int rc;

    // coap_dump(buf, buflen, false);

    if (0 != (rc = coap_parseHeader(&pkt->hdr, buf, buflen)))
        return rc;
//    coap_dumpHeader(&hdr);
    if (0 != (rc = coap_parseToken(&pkt->tok, &pkt->hdr, buf, buflen)))
        return rc;
    pkt->numopts = MAXOPT;
    if (0 != (rc = coap_parseOptionsAndPayload(pkt->opts, &(pkt->numopts), &(pkt->payload), &pkt->hdr, buf, buflen)))
        return rc;
//    coap_dumpOptions(opts, numopt);
    return 0;
}

// options are always stored consecutively, so can return a block with same option num
const coap_option_t *coap_findOptions(const coap_packet_t *pkt, uint8_t num, uint8_t *count)
{
    // FIXME, options is always sorted, can find faster than this
    size_t i;
    const coap_option_t *first = NULL;
    *count = 0;
    for (i=0;i<pkt->numopts;i++)
    {
        if (pkt->opts[i].num == num)
        {
            if (NULL == first)
                first = &pkt->opts[i];
            (*count)++;
        }
        else
        {
            if (NULL != first)
                break;
        }
    }
    return first;
}

int coap_buffer_to_string(char *strbuf, size_t strbuflen, const coap_buffer_t *buf)
{
    if (buf->len+1 > strbuflen)
        return COAP_ERR_BUFFER_TOO_SMALL;
    c_memcpy(strbuf, buf->p, buf->len);
    strbuf[buf->len] = 0;
    return 0;
}

int coap_build(uint8_t *buf, size_t *buflen, const coap_packet_t *pkt)
{
    size_t opts_len = 0, hdr_len = 0, tok_len = 0;
    size_t i;
    uint8_t *p = buf;
    size_t left = *buflen;
    uint16_t running_delta = 0;

    hdr_len = coap_buildHeader(&(pkt->hdr), buf, *buflen);
    p += hdr_len;
    left -= hdr_len;

    tok_len = coap_buildToken(&(pkt->tok), &(pkt->hdr), buf, *buflen);
    p += tok_len;
    left -= tok_len;

    for (i=0;i<pkt->numopts;i++)
    {
        uint8_t len, delta = 0;
        uint16_t optDelta = 0;
        int rc = 0;

        if (((size_t)(p-buf)) > *buflen)
             return COAP_ERR_BUFFER_TOO_SMALL;
        optDelta = pkt->opts[i].num - running_delta;

        rc = coap_buildOptionHeader(optDelta, pkt->opts[i].buf.len, p, left);
        p += rc;
        left -= rc;

        c_memcpy(p, pkt->opts[i].buf.p, pkt->opts[i].buf.len);
        p += pkt->opts[i].buf.len;
        left -= pkt->opts[i].buf.len;
        running_delta = pkt->opts[i].num;
    }

    opts_len = (p - buf) - 4;   // number of bytes used by options

    if (pkt->payload.len > 0)
    {
        if (*buflen < 4 + 1 + pkt->payload.len + opts_len)
            return COAP_ERR_BUFFER_TOO_SMALL;
        buf[4 + opts_len] = 0xFF;  // payload marker
        c_memcpy(buf+5 + opts_len, pkt->payload.p, pkt->payload.len);
        *buflen = opts_len + 5 + pkt->payload.len;
    }
    else
        *buflen = opts_len + 4;
    return 0;
}

void coap_option_nibble(uint32_t value, uint8_t *nibble)
{
    if (value<13)
    {
        *nibble = (0xFF & value);
    }
    else
    if (value<=0xFF+13)
    {
        *nibble = 13;
    } else if (value<=0xFFFF+269)
    {
        *nibble = 14;
    }
}

int coap_make_response(coap_rw_buffer_t *scratch, coap_packet_t *pkt, const uint8_t *content, size_t content_len, uint8_t msgid_hi, uint8_t msgid_lo, const coap_buffer_t* tok, coap_responsecode_t rspcode, coap_content_type_t content_type)
{
    pkt->hdr.ver = 0x01;
    pkt->hdr.t = COAP_TYPE_ACK;
    pkt->hdr.tkl = 0;
    pkt->hdr.code = rspcode;
    pkt->hdr.id[0] = msgid_hi;
    pkt->hdr.id[1] = msgid_lo;
    pkt->numopts = 1;

    // need token in response
    if (tok) {
        pkt->hdr.tkl = tok->len;
        pkt->tok = *tok;
    }

    // safe because 1 < MAXOPT
    pkt->opts[0].num = COAP_OPTION_CONTENT_FORMAT;
    pkt->opts[0].buf.p = scratch->p;
    if (scratch->len < 2)
        return COAP_ERR_BUFFER_TOO_SMALL;
    scratch->p[0] = ((uint16_t)content_type & 0xFF00) >> 8;
    scratch->p[1] = ((uint16_t)content_type & 0x00FF);
    pkt->opts[0].buf.len = 2;
    pkt->payload.p = content;
    pkt->payload.len = content_len;
    return 0;
}


unsigned int coap_encode_var_bytes(unsigned char *buf, unsigned int val) {
  unsigned int n, i;

  for (n = 0, i = val; i && n < sizeof(val); ++n)
    i >>= 8;

  i = n;
  while (i--) {
    buf[i] = val & 0xff;
    val >>= 8;
  }

  return n;
}

static uint8_t _token_data[4] = {'n','o','d','e'};
coap_buffer_t the_token = { _token_data, 4 };
static unsigned short message_id;

int coap_make_request(coap_rw_buffer_t *scratch, coap_packet_t *pkt, coap_msgtype_t t, coap_method_t m, coap_uri_t *uri, const uint8_t *payload, size_t payload_len)
{
    int res;
    pkt->hdr.ver = 0x01;
    pkt->hdr.t = t;
    pkt->hdr.tkl = 0;
    pkt->hdr.code = m;
    pkt->hdr.id[0] = (message_id >> 8) & 0xFF;  //msgid_hi;
    pkt->hdr.id[1] = message_id & 0xFF; //msgid_lo;
    message_id++;
    NODE_DBG("message_id: %d.\n", message_id);
    pkt->numopts = 0;

    if (the_token.len) {
        pkt->hdr.tkl = the_token.len;
        pkt->tok = the_token;
    }

    if (scratch->len < 2)   // TBD...
        return COAP_ERR_BUFFER_TOO_SMALL;

    uint8_t *saved = scratch->p;

    /* split arg into Uri-* options */
    // const char *addr = uri->host.s;
    // if(uri->host.length && (c_strlen(addr) != uri->host.length || c_memcmp(addr, uri->host.s, uri->host.length) != 0)){
    if(uri->host.length){
        /* add Uri-Host */
        // addr is destination address
        pkt->opts[pkt->numopts].num = COAP_OPTION_URI_HOST;
        pkt->opts[pkt->numopts].buf.p = uri->host.s;
        pkt->opts[pkt->numopts].buf.len = uri->host.length;
        pkt->numopts++;
    }

    if (uri->port != COAP_DEFAULT_PORT) {
        pkt->opts[pkt->numopts].num = COAP_OPTION_URI_PORT;
        res = coap_encode_var_bytes(scratch->p, uri->port);
        pkt->opts[pkt->numopts].buf.len = res;
        pkt->opts[pkt->numopts].buf.p = scratch->p;
        scratch->p += res;
        scratch->len -= res;
        pkt->numopts++;
    }

    if (uri->path.length) {
        res = coap_split_path(scratch, pkt, uri->path.s, uri->path.length);
    }

    if (uri->query.length) {
        res = coap_split_query(scratch, pkt, uri->query.s, uri->query.length);
    }

    pkt->payload.p = payload;
    pkt->payload.len = payload_len;
    scratch->p = saved;     // save back the pointer.
    return 0;
}

// FIXME, if this looked in the table at the path before the method then
// it could more easily return 405 errors
int coap_handle_req(coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt)
{
    const coap_option_t *opt;
    int i;
    uint8_t count;
    const coap_endpoint_t *ep = endpoints;

    while(NULL != ep->handler)
    {
        if (ep->method != inpkt->hdr.code)
            goto next;
        if (NULL != (opt = coap_findOptions(inpkt, COAP_OPTION_URI_PATH, &count)))
        {
            // if (count != ep->path->count)
            if ((count != ep->path->count ) && (count != ep->path->count + 1)) // +1 for /f/[function], /v/[variable]
                goto next;
            for (i=0;i<ep->path->count;i++)
            {
                if (opt[i].buf.len != c_strlen(ep->path->elems[i]))
                    goto next;
                if (0 != c_memcmp(ep->path->elems[i], opt[i].buf.p, opt[i].buf.len))
                    goto next;
            }
            // pre-path match!
            if (count==ep->path->count+1 && ep->user_entry == NULL)
                goto next;
            return ep->handler(ep, scratch, inpkt, outpkt, inpkt->hdr.id[0], inpkt->hdr.id[1]);
        }
next:
        ep++;
    }

    coap_make_response(scratch, outpkt, NULL, 0, inpkt->hdr.id[0], inpkt->hdr.id[1], &inpkt->tok, COAP_RSPCODE_NOT_FOUND, COAP_CONTENTTYPE_NONE);

    return 0;
}

void coap_setup(void)
{
    message_id = (unsigned short)os_random();      // calculate only once
}

int
check_token(coap_packet_t *pkt) {
  return pkt->tok.len == the_token.len && c_memcmp(pkt->tok.p, the_token.p, the_token.len) == 0;
}
