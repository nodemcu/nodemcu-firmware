/* strbuf - String buffer routines
 *
 * Copyright (c) 2010-2012  Mark Pulford <mark@kyne.com.au>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "c_stdio.h"
#include "c_stdlib.h"
#include "c_stdarg.h"
#include "c_string.h"

#include "strbuf.h"
#include "cjson_mem.h"

int strbuf_init(strbuf_t *s, int len)
{
    int size;

    if (len <= 0)
        size = STRBUF_DEFAULT_SIZE;
    else
        size = len + 1;         /* \0 terminator */

    s->buf = NULL;
    s->size = size;
    s->length = 0;
    s->increment = STRBUF_DEFAULT_INCREMENT;
    s->dynamic = 0;
    s->reallocs = 0;
    s->debug = 0;

    s->buf = (char *)cjson_mem_malloc(size);
    if (!s->buf){
        NODE_ERR("not enough memory\n");
        return -1;
    }

    strbuf_ensure_null(s);
	return 0;
}

strbuf_t *strbuf_new(int len)
{
    strbuf_t *s;

    s = (strbuf_t *)cjson_mem_malloc(sizeof(strbuf_t));
    if (!s){
        NODE_ERR("not enough memory\n");
        return NULL;
    }

    strbuf_init(s, len);

    /* Dynamic strbuf allocation / deallocation */
    s->dynamic = 1;

    return s;
}

int strbuf_set_increment(strbuf_t *s, int increment)
{
    /* Increment > 0:  Linear buffer growth rate
     * Increment < -1: Exponential buffer growth rate */
    if (increment == 0 || increment == -1){
        NODE_ERR("BUG: Invalid string increment");
        return -1;
    }

    s->increment = increment;
	return 0;
}

static inline void debug_stats(strbuf_t *s)
{
    if (s->debug) {
        NODE_ERR("strbuf(%lx) reallocs: %d, length: %d, size: %d\n",
                (long)s, s->reallocs, s->length, s->size);
    }
}

/* If strbuf_t has not been dynamically allocated, strbuf_free() can
 * be called any number of times strbuf_init() */
void strbuf_free(strbuf_t *s)
{
    debug_stats(s);

    if (s->buf) {
        c_free(s->buf);
        s->buf = NULL;
    }
    if (s->dynamic)
        c_free(s);
}

char *strbuf_free_to_string(strbuf_t *s, int *len)
{
    char *buf;

    debug_stats(s);

    strbuf_ensure_null(s);

    buf = s->buf;
    if (len)
        *len = s->length;

    if (s->dynamic)
        c_free(s);

    return buf;
}

static int calculate_new_size(strbuf_t *s, int len)
{
    int reqsize, newsize;

    if (len <= 0){
        NODE_ERR("BUG: Invalid strbuf length requested");
        return 0;
    }

    /* Ensure there is room for optional NULL termination */
    reqsize = len + 1;

    /* If the user has requested to shrink the buffer, do it exactly */
    if (s->size > reqsize)
        return reqsize;

    newsize = s->size;
    if (s->increment < 0) {
        /* Exponential sizing */
        while (newsize < reqsize)
            newsize *= -s->increment;
    } else {
        /* Linear sizing */
        newsize = (((reqsize -1) / s->increment) + 1) * s->increment;
    }

    return newsize;
}


/* Ensure strbuf can handle a string length bytes long (ignoring NULL
 * optional termination). */
int strbuf_resize(strbuf_t *s, int len)
{
    int newsize;

    newsize = calculate_new_size(s, len);

    if (s->debug > 1) {
        NODE_ERR("strbuf(%lx) resize: %d => %d\n",
                (long)s, s->size, newsize);
    }

    s->buf = (char *)cjson_mem_realloc(s->buf, newsize);
    if (!s->buf){
        NODE_ERR("not enough memory");
        return -1;
    }
	s->size = newsize;
    s->reallocs++;
	return 0;
}

void strbuf_append_string(strbuf_t *s, const char *str)
{
    int space, i;

    space = strbuf_empty_length(s);

    for (i = 0; str[i]; i++) {
        if (space < 1) {
            strbuf_resize(s, s->length + 1);
            space = strbuf_empty_length(s);
        }

        s->buf[s->length] = str[i];
        s->length++;
        space--;
    }
}
#if 0
/* strbuf_append_fmt() should only be used when an upper bound
 * is known for the output string. */
void strbuf_append_fmt(strbuf_t *s, int len, const char *fmt, ...)
{
    va_list arg;
    int fmt_len;

    strbuf_ensure_empty_length(s, len);

    va_start(arg, fmt);
    fmt_len = vsnprintf(s->buf + s->length, len, fmt, arg);
    va_end(arg);

    if (fmt_len < 0)
        die("BUG: Unable to convert number");  /* This should never happen.. */

    s->length += fmt_len;
}

/* strbuf_append_fmt_retry() can be used when the there is no known
 * upper bound for the output string. */
void strbuf_append_fmt_retry(strbuf_t *s, const char *fmt, ...)
{
    va_list arg;
    int fmt_len, try;
    int empty_len;

    /* If the first attempt to append fails, resize the buffer appropriately
     * and try again */
    for (try = 0; ; try++) {
        va_start(arg, fmt);
        /* Append the new formatted string */
        /* fmt_len is the length of the string required, excluding the
         * trailing NULL */
        empty_len = strbuf_empty_length(s);
        /* Add 1 since there is also space to store the terminating NULL. */
        fmt_len = vsnprintf(s->buf + s->length, empty_len + 1, fmt, arg);
        va_end(arg);

        if (fmt_len <= empty_len)
            break;  /* SUCCESS */
        if (try > 0)
            die("BUG: length of formatted string changed");

        strbuf_resize(s, s->length + fmt_len);
    }

    s->length += fmt_len;
}
#endif
/* vi:ai et sw=4 ts=4:
 */
