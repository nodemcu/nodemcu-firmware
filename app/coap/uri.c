/* uri.c -- helper functions for URI treatment
 */

#include "c_stdio.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "c_ctype.h"

#include "coap.h"
#include "uri.h"

#ifndef assert
// #warning "assertions are disabled"
#  define assert(x) do { \
        if(!x) NODE_ERR("uri.c assert!\n");  \
    } while (0)
#endif


/** 
 * A length-safe version of strchr(). This function returns a pointer
 * to the first occurrence of @p c  in @p s, or @c NULL if not found.
 * 
 * @param s   The string to search for @p c.
 * @param len The length of @p s.
 * @param c   The character to search.
 * 
 * @return A pointer to the first occurence of @p c, or @c NULL 
 * if not found.
 */
static inline unsigned char *
strnchr(unsigned char *s, size_t len, unsigned char c) {
  while (len && *s++ != c)
    --len;
  
  return len ? s : NULL;
}

int coap_split_uri(unsigned char *str_var, size_t len, coap_uri_t *uri) {
  unsigned char *p, *q;
  int secure = 0, res = 0;

  if (!str_var || !uri)
    return -1;

  c_memset(uri, 0, sizeof(coap_uri_t));
  uri->port = COAP_DEFAULT_PORT;

  /* search for scheme */
  p = str_var;
  if (*p == '/') {
    q = p;
    goto path;
  }

  q = (unsigned char *)COAP_DEFAULT_SCHEME;
  while (len && *q && tolower(*p) == *q) {
    ++p; ++q; --len;
  }
  
  /* If q does not point to the string end marker '\0', the schema
   * identifier is wrong. */
  if (*q) {
    res = -1;
    goto error;
  }

  /* There might be an additional 's', indicating the secure version: */
  if (len && (secure = tolower(*p) == 's')) {
    ++p; --len;
  }

  q = (unsigned char *)"://";
  while (len && *q && tolower(*p) == *q) {
    ++p; ++q; --len;
  }

  if (*q) {
    res = -2;
    goto error;
  }

  /* p points to beginning of Uri-Host */
  q = p;
  if (len && *p == '[') {	/* IPv6 address reference */
    ++p;
    
    while (len && *q != ']') {
      ++q; --len;
    }

    if (!len || *q != ']' || p == q) {
      res = -3;
      goto error;
    } 

    COAP_SET_STR(&uri->host, q - p, p);
    ++q; --len;
  } else {			/* IPv4 address or FQDN */
    while (len && *q != ':' && *q != '/' && *q != '?') {
      *q = tolower(*q);
      ++q;
      --len;
    }

    if (p == q) {
      res = -3;
      goto error;
    }

    COAP_SET_STR(&uri->host, q - p, p);
  }

  /* check for Uri-Port */
  if (len && *q == ':') {
    p = ++q;
    --len;
    
    while (len && isdigit(*q)) {
      ++q;
      --len;
    }

    if (p < q) {		/* explicit port number given */
      int uri_port = 0;
    
      while (p < q)
	     uri_port = uri_port * 10 + (*p++ - '0');

      uri->port = uri_port;
    } 
  }
  
 path:		 /* at this point, p must point to an absolute path */

  if (!len)
    goto end;
  
  if (*q == '/') {
    p = ++q;
    --len;

    while (len && *q != '?') {
      ++q;
      --len;
    }
  
    if (p < q) {
      COAP_SET_STR(&uri->path, q - p, p);
      p = q;
    }
  }

  /* Uri_Query */
  if (len && *p == '?') {
    ++p;
    --len;
    COAP_SET_STR(&uri->query, len, p);
    len = 0;
  }

  end:
  return len ? -1 : 0;
  
  error:
  return res;
}

/** 
 * Calculates decimal value from hexadecimal ASCII character given in
 * @p c. The caller must ensure that @p c actually represents a valid
 * heaxdecimal character, e.g. with isxdigit(3). 
 *
 * @hideinitializer
 */
#define hexchar_to_dec(c) ((c) & 0x40 ? ((c) & 0x0F) + 9 : ((c) & 0x0F))

/** 
 * Decodes percent-encoded characters while copying the string @p seg
 * of size @p length to @p buf. The caller of this function must
 * ensure that the percent-encodings are correct (i.e. the character
 * '%' is always followed by two hex digits. and that @p buf provides
 * sufficient space to hold the result. This function is supposed to
 * be called by make_decoded_option() only.
 * 
 * @param seg     The segment to decode and copy.
 * @param length  Length of @p seg.
 * @param buf     The result buffer.
 */
void decode_segment(const unsigned char *seg, size_t length, unsigned char *buf) {

  while (length--) {

    if (*seg == '%') {
      *buf = (hexchar_to_dec(seg[1]) << 4) + hexchar_to_dec(seg[2]);
      
      seg += 2; length -= 2;
    } else {
      *buf = *seg;
    }
    
    ++buf; ++seg;
  }
}

/**
 * Runs through the given path (or query) segment and checks if
 * percent-encodings are correct. This function returns @c -1 on error
 * or the length of @p s when decoded.
 */
int check_segment(const unsigned char *s, size_t length) {

  size_t n = 0;

  while (length) {
    if (*s == '%') {
      if (length < 2 || !(isxdigit(s[1]) && isxdigit(s[2])))
        return -1;
      
      s += 2;
      length -= 2;
    }

    ++s; ++n; --length;
  }
  
  return n;
}
	 
/** 
 * Writes a coap option from given string @p s to @p buf. @p s should
 * point to a (percent-encoded) path or query segment of a coap_uri_t
 * object.  The created option will have type @c 0, and the length
 * parameter will be set according to the size of the decoded string.
 * On success, this function returns the option's size, or a value
 * less than zero on error. This function must be called from
 * coap_split_path_impl() only.
 * 
 * @param s       The string to decode.
 * @param length  The size of the percent-encoded string @p s.
 * @param buf     The buffer to store the new coap option.
 * @param buflen  The maximum size of @p buf.
 * 
 * @return The option's size, or @c -1 on error.
 *
 * @bug This function does not split segments that are bigger than 270
 * bytes.
 */
int make_decoded_option(const unsigned char *s, size_t length, 
		    unsigned char *buf, size_t buflen) {
  int res;
  size_t written;

  if (!buflen) {
    NODE_DBG("make_decoded_option(): buflen is 0!\n");
    return -1;
  }

  res = check_segment(s, length);
  if (res < 0)
    return -1;

  /* write option header using delta 0 and length res */
  // written = coap_opt_setheader(buf, buflen, 0, res);
  written = coap_buildOptionHeader(0, res, buf, buflen);

  assert(written <= buflen);

  if (!written)			/* encoding error */
    return -1;

  buf += written;		/* advance past option type/length */
  buflen -= written;

  if (buflen < (size_t)res) {
    NODE_DBG("buffer too small for option\n");
    return -1;
  }

  decode_segment(s, length, buf);

  return written + res;
}


#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

typedef void (*segment_handler_t)(unsigned char *, size_t, void *);

/** 
 * Splits the given string into segments. You should call one of the
 * macros coap_split_path() or coap_split_query() instead.
 * 
 * @param parse_iter The iterator used for tokenizing.
 * @param h      A handler that is called with every token.
 * @param data   Opaque data that is passed to @p h when called.
 * 
 * @return The number of characters that have been parsed from @p s.
 */
size_t coap_split_path_impl(coap_parse_iterator_t *parse_iter,
		     segment_handler_t h, void *data) {
  unsigned char *seg;
  size_t length;
  
  assert(parse_iter);
  assert(h);

  length = parse_iter->n;
  
  while ( (seg = coap_parse_next(parse_iter)) ) {

    /* any valid path segment is handled here: */
    h(seg, parse_iter->segment_length, data);
  }
  
  return length - (parse_iter->n - parse_iter->segment_length);
}

struct pkt_scr {
  coap_packet_t *pkt;
  coap_rw_buffer_t *scratch;
  int n;
};

void write_option(unsigned char *s, size_t len, void *data) {
  struct pkt_scr *state = (struct pkt_scr *)data;
  int res;
  assert(state);

  /* skip empty segments and those that consist of only one or two dots */
  if (memcmp(s, "..", min(len,2)) == 0)
    return;
  
  res = check_segment(s, len);
  if (res < 0){
    NODE_DBG("not a valid segment\n");
    return;
  }

  if (state->scratch->len < (size_t)res) {
    NODE_DBG("buffer too small for option\n");
    return;
  }

  decode_segment(s, len, state->scratch->p);

  if (res > 0) {
    state->pkt->opts[state->pkt->numopts].buf.p = state->scratch->p;
    state->pkt->opts[state->pkt->numopts].buf.len = res;
    state->scratch->p += res;
    state->scratch->len -= res;
    state->pkt->numopts++;
    state->n++;
  }
}

int coap_split_path(coap_rw_buffer_t *scratch, coap_packet_t *pkt, const unsigned char *s, size_t length) {
  struct pkt_scr tmp = { pkt, scratch, 0 };
  coap_parse_iterator_t pi;

  coap_parse_iterator_init((unsigned char *)s, length, 
         '/', (unsigned char *)"?#", 2, &pi);
  coap_split_path_impl(&pi, write_option, &tmp);

  int i;
  for(i=0;i<tmp.n;i++){
    pkt->opts[pkt->numopts - i - 1].num = COAP_OPTION_URI_PATH;
  }

  return tmp.n;
}

int coap_split_query(coap_rw_buffer_t *scratch, coap_packet_t *pkt, const unsigned char *s, size_t length) {
  struct pkt_scr tmp = { pkt, scratch, 0 };
  coap_parse_iterator_t pi;

  coap_parse_iterator_init((unsigned char *)s, length, 
         '&', (unsigned char *)"#", 1, &pi);

  coap_split_path_impl(&pi, write_option, &tmp);

  int i;
  for(i=0;i<tmp.n;i++){
    pkt->opts[pkt->numopts - i - 1].num = COAP_OPTION_URI_QUERY;
  }

  return tmp.n;
}

#define URI_DATA(uriobj) ((unsigned char *)(uriobj) + sizeof(coap_uri_t))

coap_uri_t * coap_new_uri(const unsigned char *uri, unsigned int length) {
  unsigned char *result;

  result = (unsigned char *)c_malloc(length + 1 + sizeof(coap_uri_t));

  if (!result)
    return NULL;

  c_memcpy(URI_DATA(result), uri, length);
  URI_DATA(result)[length] = '\0'; /* make it zero-terminated */

  if (coap_split_uri(URI_DATA(result), length, (coap_uri_t *)result) < 0) {
    c_free(result);
    return NULL;
  }
  return (coap_uri_t *)result;
}

/* iterator functions */

coap_parse_iterator_t * coap_parse_iterator_init(unsigned char *s, size_t n, 
			 unsigned char separator,
			 unsigned char *delim, size_t dlen,
			 coap_parse_iterator_t *pi) {
  assert(pi);
  assert(separator);

  pi->separator = separator;
  pi->delim = delim;
  pi->dlen = dlen;
  pi->pos = s;
  pi->n = n;
  pi->segment_length = 0;

  return pi;
}

unsigned char * coap_parse_next(coap_parse_iterator_t *pi) {
  unsigned char *p;

  if (!pi)
    return NULL;

  /* proceed to the next segment */
  pi->n -= pi->segment_length;
  pi->pos += pi->segment_length;
  pi->segment_length = 0;

  /* last segment? */
  if (!pi->n || strnchr(pi->delim, pi->dlen, *pi->pos)) {
    pi->pos = NULL;
    return NULL;
  }

  /* skip following separator (the first segment might not have one) */
  if (*pi->pos == pi->separator) {
    ++pi->pos;
    --pi->n;
  }

  p = pi->pos;

  while (pi->segment_length < pi->n && *p != pi->separator &&
	 !strnchr(pi->delim, pi->dlen, *p)) {
    ++p;
    ++pi->segment_length;
  }

  if (!pi->n) {
    pi->pos = NULL;
    pi->segment_length = 0;
  }

  return pi->pos;
}
