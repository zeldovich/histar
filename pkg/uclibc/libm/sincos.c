#include <math.h>

void
sincos(double x, double *sinx, double *cosx)
{
    *sinx = sin(x);
    *cosx = cos(x);
}

void
sincosf(float x, float *sinx, float *cosx)
{
    *sinx = sinf(x);
    *cosx = cosf(x);
}

void
sincosl(long double x, long double *sinx, long double *cosx)
{
    *sinx = sinl(x);
    *cosx = cosl(x);
}
