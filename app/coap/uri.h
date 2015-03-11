/* uri.h -- helper functions for URI treatment
 *
 * Copyright (C) 2010,2011 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see
 * README for terms of use. 
 */

#ifndef _COAP_URI_H_
#define _COAP_URI_H_

#define COAP_DEFAULT_SCHEME        "coap" /* the default scheme for CoAP URIs */
#define COAP_DEFAULT_PORT   5683

#include "str.h"

/** Representation of parsed URI. Components may be filled from a
 * string with coap_split_uri() and can be used as input for
 * option-creation functions. */
typedef struct {
  str host;			/**< host part of the URI */
  unsigned short port;		/**< The port in host byte order */
  str path;			/**< Beginning of the first path segment. 
				   Use coap_split_path() to create Uri-Path options */
  str query;			/**<  The query part if present */
} coap_uri_t;

/**
 * Creates a new coap_uri_t object from the specified URI. Returns the new
 * object or NULL on error. The memory allocated by the new coap_uri_t
 * must be released using coap_free().
 * @param uri The URI path to copy.
 * @para length The length of uri.
 *
 * @return New URI object or NULL on error.
 */
coap_uri_t *coap_new_uri(const unsigned char *uri, unsigned int length);

/**
 * @defgroup uri_parse URI Parsing Functions
 *
 * CoAP PDUs contain normalized URIs with their path and query split into
 * multiple segments. The functions in this module help splitting strings.
 * @{
 */

/** 
 * Iterator to for tokenizing a URI path or query. This structure must
 * be initialized with coap_parse_iterator_init(). Call
 * coap_parse_next() to walk through the tokens.
 *
 * @code
 * unsigned char *token;
 * coap_parse_iterator_t pi;
 * coap_parse_iterator_init(uri.path.s, uri.path.length, '/', "?#", 2, &pi);
 *
 * while ((token = coap_parse_next(&pi))) {
 *   ... do something with token ...
 * }
 * @endcode
 */
typedef struct {
  size_t n;			/**< number of remaining characters in buffer */
  unsigned char separator;	/**< segment separators */
  unsigned char *delim; 	/**< delimiters where to split the string */
  size_t dlen;			/**< length of separator */
  unsigned char *pos;		/**< current position in buffer */
  size_t segment_length;	/**< length of current segment */
} coap_parse_iterator_t;

/** 
 * Initializes the given iterator @p pi. 
 * 
 * @param s         The string to tokenize.
 * @param n         The length of @p s.
 * @param separator The separator character that delimits tokens.
 * @param delim     A set of characters that delimit @s.
 * @param dlen      The length of @p delim.
 * @param pi        The iterator object to initialize.
 * 
 * @return The initialized iterator object @p pi.
 */
coap_parse_iterator_t *
coap_parse_iterator_init(unsigned char *s, size_t n, 
			 unsigned char separator,
			 unsigned char *delim, size_t dlen,
			 coap_parse_iterator_t *pi);

/** 
 * Updates the iterator @p pi to point to the next token. This
 * function returns a pointer to that token or @c NULL if no more
 * tokens exist. The contents of @p pi will be updated. In particular,
 * @c pi->segment_length specifies the length of the current token, @c
 * pi->pos points to its beginning.
 * 
 * @param pi The iterator to update.
 * 
 * @return The next token or @c NULL if no more tokens exist.
 */
unsigned char *coap_parse_next(coap_parse_iterator_t *pi);

/** 
 * Parses a given string into URI components. The identified syntactic
 * components are stored in the result parameter @p uri. Optional URI
 * components that are not specified will be set to { 0, 0 }, except
 * for the port which is set to @c COAP_DEFAULT_PORT. This function
 * returns @p 0 if parsing succeeded, a value less than zero
 * otherwise.
 * 
 * @param str_var The string to split up.
 * @param len     The actual length of @p str_var
 * @param uri     The coap_uri_t object to store the result.
 * @return @c 0 on success, or < 0 on error.
 *
 * @note The host name part will be converted to lower case by this
 * function.
 */
int
coap_split_uri(unsigned char *str_var, size_t len, coap_uri_t *uri);

/** 
 * Splits the given URI path into segments. Each segment is preceded
 * by an option pseudo-header with delta-value 0 and the actual length
 * of the respective segment after percent-decoding.
 * 
 * @param s      The path string to split. 
 * @param length The actual length of @p s.
 * @param buf    Result buffer for parsed segments. 
 * @param buflen Maximum length of @p buf. Will be set to the actual number
 * of bytes written into buf on success.
 * 
 * @return The number of segments created or @c -1 on error.
 */
#if 0
int coap_split_path(const unsigned char *s, size_t length, 
		    unsigned char *buf, size_t *buflen);
#else
int
coap_split_path(coap_rw_buffer_t *scratch, coap_packet_t *pkt, 
        const unsigned char *s, size_t length);
#endif
/** 
 * Splits the given URI query into segments. Each segment is preceded
 * by an option pseudo-header with delta-value 0 and the actual length
 * of the respective query term.
 * 
 * @param s      The query string to split. 
 * @param length The actual length of @p s.
 * @param buf    Result buffer for parsed segments. 
 * @param buflen Maximum length of @p buf. Will be set to the actual number
 * of bytes written into buf on success.
 * 
 * @return The number of segments created or @c -1 on error.
 *
 * @bug This function does not reserve additional space for delta > 12.
 */
#if 0
int coap_split_query(const unsigned char *s, size_t length, 
		     unsigned char *buf, size_t *buflen);
#else 
int coap_split_query(coap_rw_buffer_t *scratch, coap_packet_t *pkt, 
          const unsigned char *s, size_t length);
#endif
/** @} */

#endif /* _COAP_URI_H_ */
