#include "c_math.h"
#include "c_types.h"
#include "user_config.h"

double floor(double x)
{
    return (double) (x < 0.f ? ((int) x == x ? x : (((int) x) - 1)) : ((int) x)); 
}

#define MAXEXP 2031     /* (MAX_EXP * 16) - 1           */
#define MINEXP -2047        /* (MIN_EXP * 16) - 1           */
#define HUGE MAXFLOAT

double a1[] ICACHE_STORE_ATTR ICACHE_RODATA_ATTR =
{
    1.0,
    0.95760328069857365,
    0.91700404320467123,
    0.87812608018664974,
    0.84089641525371454,
    0.80524516597462716,
    0.77110541270397041,
    0.73841307296974966,
    0.70710678118654752,
    0.67712777346844637,
    0.64841977732550483,
    0.62092890603674203,
    0.59460355750136054,
    0.56939431737834583,
    0.54525386633262883,
    0.52213689121370692,
    0.50000000000000000
};
double a2[] ICACHE_STORE_ATTR ICACHE_RODATA_ATTR =
{
    0.24114209503420288E-17,
    0.92291566937243079E-18,
    -0.15241915231122319E-17,
    -0.35421849765286817E-17,
    -0.31286215245415074E-17,
    -0.44654376565694490E-17,
    0.29306999570789681E-17,
    0.11260851040933474E-17
};
double p1 ICACHE_STORE_ATTR ICACHE_RODATA_ATTR = 0.833333333333332114e-1;
double p2 ICACHE_STORE_ATTR ICACHE_RODATA_ATTR = 0.125000000005037992e-1;
double p3 ICACHE_STORE_ATTR ICACHE_RODATA_ATTR = 0.223214212859242590e-2;
double p4 ICACHE_STORE_ATTR ICACHE_RODATA_ATTR = 0.434457756721631196e-3;
double q1 ICACHE_STORE_ATTR ICACHE_RODATA_ATTR = 0.693147180559945296e0;
double q2 ICACHE_STORE_ATTR ICACHE_RODATA_ATTR = 0.240226506959095371e0;
double q3 ICACHE_STORE_ATTR ICACHE_RODATA_ATTR = 0.555041086640855953e-1;
double q4 ICACHE_STORE_ATTR ICACHE_RODATA_ATTR = 0.961812905951724170e-2;
double q5 ICACHE_STORE_ATTR ICACHE_RODATA_ATTR = 0.133335413135857847e-2;
double q6 ICACHE_STORE_ATTR ICACHE_RODATA_ATTR = 0.154002904409897646e-3;
double q7 ICACHE_STORE_ATTR ICACHE_RODATA_ATTR = 0.149288526805956082e-4;
double k ICACHE_STORE_ATTR ICACHE_RODATA_ATTR = 0.442695040888963407;

double pow(double x, double y)
{
    double frexp(), g, ldexp(), r, u1, u2, v, w, w1, w2, y1, y2, z;
    int iw1, m, p;
    bool flipsignal = false;

    if (y == 0.0)
        return (1.0);
    if (x <= 0.0)
    {
        if (x == 0.0)
        {
            if (y > 0.0)
                return 0.0;
            //cmemsg(FP_POWO, &y);
            //return(HUGE);
        }
        else
        {
            //cmemsg(FP_POWN, &x);
            x = -x;

            if (y != (int) y) { // if y is fractional, then this woud result in a complex number
                return NAN;
            }
            flipsignal = ((int) y) & 1;
        }
    }
    g = frexp(x, &m);
    p = 0;
    if (g <= a1[8])
        p = 8;
    if (g <= a1[p + 4])
        p += 4;
    if (g <= a1[p + 2])
        p += 2;
    p++;
    z = ((g - a1[p]) - a2[p / 2]) / (g + a1[p]);
    z += z;
    v = z * z;
    r = (((p4 * v + p3) * v + p2) * v + p1) * v * z;
    r += k * r;
    u2 = (r + z * k) + z;
    u1 = 0.0625 * (double)(16 * m - p);
    y1 = 0.0625 * (double)((int)(16.0 * y));
    y2 = y - y1;
    w = u2 * y + u1 * y2;
    w1 = 0.0625 * (double)((int)(16.0 * w));
    w2 = w - w1;
    w = w1 + u1 * y1;
    w1 = 0.0625 * (double)((int)(16.0 * w));
    w2 += (w - w1);
    w = 0.0625 * (double)((int)(16.0 * w2));
    iw1 = 16.0 * (w1 + w);
    w2 -= w;
    while (w2 > 0.0)
    {
        iw1++;
        w2 -= 0.0625;
    }
    if (iw1 > MAXEXP)
    {
        //cmemsg(FP_POWO, &y);
        return (HUGE);
    }
    if (iw1 < MINEXP)
    {
        //cmemsg(FP_POWU, &y);
        return (0.0);
    }
    m = iw1 / 16;
    if (iw1 >= 0)
        m++;
    p = 16 * m - iw1;
    z = ((((((q7 * w2 + q6) * w2 + q5) * w2 + q4) * w2 + q3) * w2 + q2) * w2 + q1) * w2;
    z = a1[p] + a1[p] * z;
    double res = ldexp(z, m);
    return flipsignal ? -res : res;
}

#if 0
#ifndef __math_68881
double atan(double x)
{
    return x;
}
double cos(double x)
{
    return x;
}
double sin(double x)
{
    return x;
}
double tan(double x)
{
    return x;
}
double tanh(double x)
{
    return x;
}
double frexp(double x, int *y)
{
    return x;
}
double modf(double x, double *y)
{
    return x;
}
double ceil(double x)
{
    return x;
}
double fabs(double x)
{
    return x;
}
double floor(double x)
{
    return x;
}
#endif /* ! defined (__math_68881) */

/* Non reentrant ANSI C functions.  */

#ifndef _REENT_ONLY
#ifndef __math_68881
double acos(double x)
{
    return x;
}
double asin(double x)
{
    return x;
}
double atan2(double x, double y)
{
    return x;
}
double cosh(double x)
{
    return x;
}
double sinh(double x)
{
    return x;
}
double exp(double x)
{
    return x;
}
double ldexp(double x, int y)
{
    return x;
}
double log(double x)
{
    return x;
}
double log10(double x)
{
    return x;
}
double pow(double x, double y)
{
    return x;
}
double sqrt(double x)
{
    return x;
}
double fmod(double x, double y)
{
    return x;
}
#endif /* ! defined (__math_68881) */
#endif /* ! defined (_REENT_ONLY) */
#endif
