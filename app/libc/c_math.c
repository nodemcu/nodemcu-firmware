#include "c_math.h"
#include "c_types.h"

#if 0
#ifndef __math_68881
double atan(double x){
	return x;
}
double cos(double x){
	return x;
}
double sin(double x){
	return x;
}
double tan(double x){
	return x;
}
double tanh(double x){
	return x;
}
double frexp(double x, int *y){
	return x;
}
double modf(double x, double *y){
	return x;
}
double ceil(double x){
	return x;
}
double fabs(double x){
	return x;
}
double floor(double x){
	return x;
}
#endif /* ! defined (__math_68881) */

/* Non reentrant ANSI C functions.  */

#ifndef _REENT_ONLY
#ifndef __math_68881
double acos(double x){
	return x;
}
double asin(double x){
	return x;
}
double atan2(double x, double y){
	return x;
}
double cosh(double x){
	return x;
}
double sinh(double x){
	return x;
}
double exp(double x){
	return x;
}
double ldexp(double x, int y){
	return x;
}
double log(double x){
	return x;
}
double log10(double x){
	return x;
}
double pow(double x, double y){
	return x;
}
double sqrt(double x){
	return x;
}
double fmod(double x, double y){
	return x;
}
#endif /* ! defined (__math_68881) */
#endif /* ! defined (_REENT_ONLY) */
#endif
