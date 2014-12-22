#include "c_math.h"
#include "c_types.h"

#if 0
#ifndef __math_68881
double ICACHE_FLASH_ATTR atan(double x){
	return x;
}
double ICACHE_FLASH_ATTR cos(double x){
	return x;
}
double ICACHE_FLASH_ATTR sin(double x){
	return x;
}
double ICACHE_FLASH_ATTR tan(double x){
	return x;
}
double ICACHE_FLASH_ATTR tanh(double x){
	return x;
}
double ICACHE_FLASH_ATTR frexp(double x, int *y){
	return x;
}
double ICACHE_FLASH_ATTR modf(double x, double *y){
	return x;
}
double ICACHE_FLASH_ATTR ceil(double x){
	return x;
}
double ICACHE_FLASH_ATTR fabs(double x){
	return x;
}
double ICACHE_FLASH_ATTR floor(double x){
	return x;
}
#endif /* ! defined (__math_68881) */

/* Non reentrant ANSI C functions.  */

#ifndef _REENT_ONLY
#ifndef __math_68881
double ICACHE_FLASH_ATTR acos(double x){
	return x;
}
double ICACHE_FLASH_ATTR asin(double x){
	return x;
}
double ICACHE_FLASH_ATTR atan2(double x, double y){
	return x;
}
double ICACHE_FLASH_ATTR cosh(double x){
	return x;
}
double ICACHE_FLASH_ATTR sinh(double x){
	return x;
}
double ICACHE_FLASH_ATTR exp(double x){
	return x;
}
double ICACHE_FLASH_ATTR ldexp(double x, int y){
	return x;
}
double ICACHE_FLASH_ATTR log(double x){
	return x;
}
double ICACHE_FLASH_ATTR log10(double x){
	return x;
}
double ICACHE_FLASH_ATTR pow(double x, double y){
	return x;
}
double ICACHE_FLASH_ATTR sqrt(double x){
	return x;
}
double ICACHE_FLASH_ATTR fmod(double x, double y){
	return x;
}
#endif /* ! defined (__math_68881) */
#endif /* ! defined (_REENT_ONLY) */
#endif
