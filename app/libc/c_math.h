#ifndef  _C_MATH_H_
#define  _C_MATH_H_
#include <math.h>

double floor(double);
double pow(double, double);

#if 0
#ifndef HUGE_VAL
  #define HUGE_VAL (1.0e99)
 #endif

 #ifndef HUGE_VALF
  #define HUGE_VALF (1.0e999999999F)
 #endif

 #if !defined(HUGE_VALL)  &&  defined(_HAVE_LONG_DOUBLE)
  #define HUGE_VALL (1.0e999999999L)
 #endif

 #if !defined(INFINITY)
  #define INFINITY (HUGE_VALF)
 #endif


/* Reentrant ANSI C functions.  */

#ifndef __math_68881
// double atan(double);
// double cos(double);
// double sin(double);
// double tan(double);
// double tanh(double);
// double frexp(double, int *);
// double modf(double, double *);
// double ceil(double);
// double fabs(double);
// double floor(double);
#endif /* ! defined (__math_68881) */

/* Non reentrant ANSI C functions.  */

#ifndef _REENT_ONLY
#ifndef __math_68881
// double acos(double);
// double asin(double);
// double atan2(double, double);
// double cosh(double);
// double sinh(double);
// double exp(double);
// double ldexp(double, int);
// double log(double);
// double log10(double);
// double pow(double, double);
// double sqrt(double);
// double fmod(double, double);
#endif /* ! defined (__math_68881) */
#endif /* ! defined (_REENT_ONLY) */

#endif

#endif /* _MATH_H_ */
