#ifndef __c_stddef_h
#define __c_stddef_h

typedef signed int ptrdiff_t;

#if !defined(offsetof)
#define offsetof(s, m)   (size_t)&(((s *)0)->m)
#endif 

#if !defined(__size_t)
  #define __size_t 1
  typedef unsigned int size_t;   /* others (e.g. <stdio.h>) also define */
   /* the unsigned integral type of the result of the sizeof operator. */
#endif

#undef NULL  /* others (e.g. <stdio.h>) also define */
#define NULL 0
   /* null pointer constant. */

#endif

/* end of c_stddef.h */

