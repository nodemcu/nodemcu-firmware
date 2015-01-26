#include "c_stdio.h"
// #include "driver/uart.h"

int c_stdin = 999;
int c_stdout = 1000;
int c_stderr = 1001;

// FILE *c_fopen(const char *_name, const char *_type){
// }
// FILE *c_freopen(const char *_name, const char *_type, FILE *_f){
// }
// FILE *c_tmpfile(void){
// }

// int    c_putchar(int c){
// }

// int    c_printf(const char *c, ...){
// }

// int c_sprintf(char *c, const char *s, ...){
// }

// int    c_fprintf(FILE *f, const char *s, ...){
// }
// int    c_fscanf(FILE *f, const char *s, ...){
// }
// int    c_fclose(FILE *f){
// }
// int    c_fflush(FILE *f){
// }
// int    c_setvbuf(FILE *f, char *c, int d, size_t t){
// }
// void c_clearerr(FILE *f){
// }
// int    c_fseek(FILE *f, long l, int d){
// }
// long c_ftell( FILE *f){
// }
// int    c_fputs(const char *c, FILE *f){
// }
// char *c_fgets(char *c, int d, FILE *f){
// }
// int    c_ungetc(int d, FILE *f){
// }
// size_t c_fread(void *p, size_t _size, size_t _n, FILE *f){
// }
// size_t c_fwrite(const void *p, size_t _size, size_t _n, FILE *f){
// }
// int    c_feof(FILE *f){
// }
// int    c_ferror(FILE *f){
// }
// int    c_getc(FILE *f){
// }

#if defined( LUA_NUMBER_INTEGRAL )

#else

/*
File: printf.c

Copyright (c) 2004,2012 Kustaa Nyholm / SpareTimeLabs

All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or other
materials provided with the distribution.

Neither the name of the Kustaa Nyholm or SpareTimeLabs nor the names of its
contributors may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.

----------------------------------------------------------------------

This library is realy just two files: 'printf.h' and 'printf.c'.

They provide a simple and small (+200 loc) printf functionality to
be used in embedded systems.

I've found them so usefull in debugging that I do not bother with a
debugger at all.

They are distributed in source form, so to use them, just compile them
into your project.

Two printf variants are provided: printf and sprintf.

The formats supported by this implementation are: 'd' 'u' 'c' 's' 'x' 'X'.

Zero padding and field width are also supported.

If the library is compiled with 'PRINTF_SUPPORT_LONG' defined then the
long specifier is also
supported. Note that this will pull in some long math routines (pun intended!)
and thus make your executable noticably longer.

The memory foot print of course depends on the target cpu, compiler and
compiler options, but a rough guestimate (based on a H8S target) is about
1.4 kB for code and some twenty 'int's and 'char's, say 60 bytes of stack space.
Not too bad. Your milage may vary. By hacking the source code you can
get rid of some hunred bytes, I'm sure, but personally I feel the balance of
functionality and flexibility versus  code size is close to optimal for
many embedded systems.

To use the printf you need to supply your own character output function,
something like :

void putc ( void* p, char c)
    {
    while (!SERIAL_PORT_EMPTY) ;
    SERIAL_PORT_TX_REGISTER = c;
    }

Before you can call printf you need to initialize it to use your
character output function with something like:

init_printf(NULL,putc);

Notice the 'NULL' in 'init_printf' and the parameter 'void* p' in 'putc',
the NULL (or any pointer) you pass into the 'init_printf' will eventually be
passed to your 'putc' routine. This allows you to pass some storage space (or
anything realy) to the character output function, if necessary.
This is not often needed but it was implemented like that because it made
implementing the sprintf function so neat (look at the source code).

The code is re-entrant, except for the 'init_printf' function, so it
is safe to call it from interupts too, although this may result in mixed output.
If you rely on re-entrancy, take care that your 'putc' function is re-entrant!

The printf and sprintf functions are actually macros that translate to
'tfp_printf' and 'tfp_sprintf'. This makes it possible
to use them along with 'stdio.h' printf's in a single source file.
You just need to undef the names before you include the 'stdio.h'.
Note that these are not function like macros, so if you have variables
or struct members with these names, things will explode in your face.
Without variadic macros this is the best we can do to wrap these
fucnction. If it is a problem just give up the macros and use the
functions directly or rename them.

For further details see source code.

regs Kusti, 23.10.2004
*/

/*
Add lightweight %g support by vowstar, <vowstar@gmail.com>
NodeMCU Team, 26.1.2015
*/

typedef void (*putcf) (void *, char);

#ifdef PRINTF_LONG_SUPPORT

static int uli2a(unsigned long int num, unsigned int base, int uc, char *bf)
{
    int n = 0;
    unsigned long int d = 1;
    int len = 1;
    while (num / d >= base)
    {
        d *= base;
        len ++;
    }
    while (d != 0)
    {
        int dgt = num / d;
        num %= d;
        d /= base;
        if (n || dgt > 0 || d == 0)
        {
            *bf++ = dgt + (dgt < 10 ? '0' : (uc ? 'A' : 'a') - 10);
            ++n;
        }
    }
    *bf = 0;
    return len;
}

static int li2a (long num, char *bf)
{
    int len = 0;
    if (num < 0)
    {
        num = -num;
        *bf++ = '-';
        len ++;
    }
    len += uli2a(num, 10, 0, bf);
    return len;
}

#endif

static int ui2a(unsigned int num, unsigned int base, int uc, char *bf)
{
    int n = 0;
    unsigned int d = 1;
    int len = 1;
    while (num / d >= base)
    {
        d *= base;
        len ++;
    }
    while (d != 0)
    {
        int dgt = num / d;
        num %= d;
        d /= base;
        if (n || dgt > 0 || d == 0)
        {
            *bf++ = dgt + (dgt < 10 ? '0' : (uc ? 'A' : 'a') - 10);
            ++n;
        }
    }
    *bf = 0;
    return len;
}

static int i2a (int num, char *bf)
{
    int len = 0;
    if (num < 0)
    {
        num = -num;
        *bf++ = '-';
        len ++;
    }
    len += ui2a(num, 10, 0, bf);
    return len;
}

// Converts a floating point number to string.
static int d2a(double num, char *bf)
{
    int len = 0;
    double ipart = 0;
    double fpart = 0;
    double absnum = num;

    // Add sign
    if (absnum < 0)
    {
        absnum = -absnum;
        *bf++ = '-';
        // len must add 1 when return
        // but can't add at here
    }

    // Extract integer part
    ipart = (int)absnum;

    // Extract floating part
    fpart = absnum - (double)ipart;

    // convert integer part to string
#ifdef  PRINTF_LONG_SUPPORT
    len += li2a(ipart, bf);
#else
    len += i2a(ipart, bf);
#endif

#ifndef EPSILON
#define EPSILON ((double)(0.00000001))
#endif
    if (fpart < EPSILON)
    {
        // fpart is zero
    }
    else
    {
        bf += len;
        // add dot
        *bf ++ = '.';
        len += 1;
        // add zero after dot
        while (fpart < 0.1)
        {
            fpart *= 10;
            *bf ++ = '0';
            len += 1;
        }
        while ((fpart < (double)1.0 / EPSILON) && ((fpart - (int)fpart) > EPSILON))
        {
            fpart = fpart * 10;
        }
#ifdef  PRINTF_LONG_SUPPORT
        len += li2a((int)fpart, bf);
#else
        len += i2a((int)fpart, bf);
#endif
    }
#undef EPSILON
    if (num < 0)
    {
        len ++;
    }
    return len;
}

static int a2d(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    else if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    else return -1;
}

static char a2i(char ch, char **src, int base, int *nump)
{
    char *p = *src;
    int num = 0;
    int digit;
    while ((digit = a2d(ch)) >= 0)
    {
        if (digit > base) break;
        num = num * base + digit;
        ch = *p++;
    }
    *src = p;
    *nump = num;
    return ch;
}

static void putchw(void *putp, putcf putf, int n, char z, char *bf)
{
    char fc = z ? '0' : ' ';
    char ch;
    char *p = bf;
    while (*p++ && n > 0)
        n--;
    while (n-- > 0)
        putf(putp, fc);
    while ((ch = *bf++))
        putf(putp, ch);
}

void c_format(void *putp, putcf putf, char *fmt, va_list va)
{
    char bf[12];

    char ch;


    while ((ch = *(fmt++)))
    {
        if (ch != '%')
            putf(putp, ch);
        else
        {
            char lz = 0;
#ifdef  PRINTF_LONG_SUPPORT
            char lng = 0;
#endif
            int w = 0;
            ch = *(fmt++);
            if (ch == '0')
            {
                ch = *(fmt++);
                lz = 1;
            }
            if (ch >= '0' && ch <= '9')
            {
                ch = a2i(ch, &fmt, 10, &w);
            }
#ifdef  PRINTF_LONG_SUPPORT
            if (ch == 'l')
            {
                ch = *(fmt++);
                lng = 1;
            }
#endif
            switch (ch)
            {
            case 0:
                goto abort;
            case 'u' :
            {
#ifdef  PRINTF_LONG_SUPPORT
                if (lng)
                    uli2a(va_arg(va, unsigned long int), 10, 0, bf);
                else
#endif
                    ui2a(va_arg(va, unsigned int), 10, 0, bf);
                putchw(putp, putf, w, lz, bf);
                break;
            }
            case 'd' :
            {
#ifdef  PRINTF_LONG_SUPPORT
                if (lng)
                    li2a(va_arg(va, unsigned long int), bf);
                else
#endif
                    i2a(va_arg(va, int), bf);
                putchw(putp, putf, w, lz, bf);
                break;
            }
            case 'x': case 'X' :
#ifdef  PRINTF_LONG_SUPPORT
                if (lng)
                    uli2a(va_arg(va, unsigned long int), 16, (ch == 'X'), bf);
                else
#endif
                    ui2a(va_arg(va, unsigned int), 16, (ch == 'X'), bf);
                putchw(putp, putf, w, lz, bf);
                break;
            case 'g' :
            {
                d2a(va_arg(va, double), bf);
                putchw(putp, putf, w, lz, bf);
                break;
            }
            case 'c' :
                putf(putp, (char)(va_arg(va, int)));
                break;
            case 's' :
                putchw(putp, putf, w, 0, va_arg(va, char *));
                break;
            case '%' :
                putf(putp, ch);
            default:
                break;
            }
        }
    }
abort:;
}


static void putcp(void *p, char c)
{
    *(*((char **)p))++ = c;
}


void c_sprintf(char *s, char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    c_format(&s, putcp, fmt, va);
    putcp(&s, 0);
    va_end(va);
}

#endif
