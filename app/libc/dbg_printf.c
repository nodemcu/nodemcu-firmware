/*	$NetBSD: printf.c,v 1.12 1997/06/26 19:11:48 drochner Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)printf.c	8.1 (Berkeley) 6/11/93
 */

// This version uses almost no stack, and does not suffer from buffer 
// overflows. The downside is that it does not implement a wide range
// of formatting characters.

#include <c_stdlib.h>
#include <c_types.h>
#include <c_stdarg.h>
#include "driver/uart.h"

static void kprintn (void (*)(const char), uint32_t, int, int, char);
static void kdoprnt (void (*)(const char), const char *, va_list);

void
dbg_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	kdoprnt(uart0_putc, fmt, ap);
	va_end(ap);
}

void
dbg_vprintf(const char *fmt, va_list ap)
{
	kdoprnt(uart0_putc, fmt, ap);
}

void
kdoprnt(void (*put)(const char), const char *fmt, va_list ap)
{
	register char *p;
	register int ch, n;
	unsigned long ul;
	int lflag, set;
        char zwidth;
        char width;

	for (;;) {
		while ((ch = *fmt++) != '%') {
			if (ch == '\0')
				return;
			put(ch);
		}
		lflag = 0;
                width = 0;
                zwidth = ' ';
reswitch:	switch (ch = *fmt++) {
		case '\0':
			/* XXX print the last format character? */
			return;
		case 'l':
			lflag = 1;
			goto reswitch;
		case 'c':
			ch = va_arg(ap, int);
				put(ch & 0x7f);
			break;
		case 's':
			p = va_arg(ap, char *);
                        if (p == 0) {
                          p = "<null>";
                        }
			while ((ch = *p++))
				put(ch);
			break;
		case 'd':
			ul = lflag ?
			    va_arg(ap, long) : va_arg(ap, int);
			if ((long)ul < 0) {
				put('-');
				ul = -(long)ul;
			}
			kprintn(put, ul, 10, width, zwidth);
			break;
		case 'o':
			ul = lflag ?
			    va_arg(ap, uint32_t) : va_arg(ap, uint32_t);
			kprintn(put, ul, 8, width, zwidth);
			break;
		case 'u':
			ul = lflag ?
			    va_arg(ap, uint32_t) : va_arg(ap, uint32_t);
			kprintn(put, ul, 10, width, zwidth);
			break;
		case 'p':
			ul = va_arg(ap, ptrdiff_t);
			kprintn(put, ul, 16, width, zwidth);
			break;
		case 'x':
			ul = lflag ?
			    va_arg(ap, uint32_t) : va_arg(ap, uint32_t);
			kprintn(put, ul, 16, width, zwidth);
			break;
		default:
                        if (ch >= '0' && ch <= '9') {
                          if (ch == '0' && width == 0 && zwidth == ' ') {
                            zwidth = '0';
                          } else {
                            width = width * 10 + ch - '0';
                          }
                          goto reswitch;
                        }
			put('%');
			if (lflag)
				put('l');
			put(ch);
		}
	}
	va_end(ap);
}

static void
kprintn(void (*put)(const char), uint32_t ul, int base, int width, char padchar)
{
					/* hold a long in base 8 */
	char *p, buf[(sizeof(long) * 8 / 3) + 2];

	p = buf;
	do {
		*p++ = "0123456789abcdef"[ul % base];
	} while (ul /= base);
        
        while (p - buf < width--) {
          put(padchar);
        }

	do {
		put(*--p);
	} while (p > buf);
}
