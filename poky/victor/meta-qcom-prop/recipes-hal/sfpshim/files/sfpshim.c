#define _GNU_SOURCE
#include <math.h>
#include <stdlib.h>

extern int __isinf(double);
extern int __isinff(float);
extern int __isnan(double);
extern int __isnanf(float);
extern int __finitef(float);

#define SFP __attribute__((pcs("aapcs"), visibility("default")))
#define W1(r, n, t)          SFP r __sfp_##n(t a) { return n(a); }
#define W2(r, n, t1, t2)     SFP r __sfp_##n(t1 a, t2 b) { return n(a, b); }

W1(int, __finitef, float)
W1(int, __isinf, double)
W1(int, __isinff, float)
W1(int, __isnan, double)
W1(int, __isnanf, float)
W1(double, acos, double)
W1(double, asin, double)
W1(double, atan, double)
W2(double, atan2, double, double)
W1(double, ceil, double)
W1(float, ceilf, float)
W1(double, cos, double)
W1(double, cosh, double)
W1(double, exp, double)
W1(double, fabs, double)
W1(double, floor, double)
W1(float, floorf, float)
W2(double, fmin, double, double)
W2(double, fmod, double, double)
W1(double, log, double)
W1(double, log10, double)
W1(double, log2, double)
W1(long, lroundf, float)
W2(double, modf, double, double *)
W2(double, pow, double, double)
W1(float, roundf, float)
W1(double, sin, double)
W1(double, sinh, double)
W1(double, sqrt, double)
W1(float, sqrtf, float)
W2(double, strtod, const char *, char **)
W1(double, tan, double)
W1(double, tanh, double)
SFP void __sfp_sincos(double x, double *s, double *c) { sincos(x, s, c); }
SFP void __sfp_sincosf(float x, float *s, float *c) { sincosf(x, s, c); }
