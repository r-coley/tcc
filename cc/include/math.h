#ifndef TCC_STUB_MATH_H
#define TCC_STUB_MATH_H

/* Minimal math.h for this compiler.  These are declarations only; symbols
 * come from the host C runtime/libm at link time.  Keep the header limited to
 * types the compiler currently parses (float and double, not long double). */

#ifndef M_E
#define M_E        2.71828182845904523536
#endif
#ifndef M_LOG2E
#define M_LOG2E    1.44269504088896340736
#endif
#ifndef M_LOG10E
#define M_LOG10E   0.43429448190325182765
#endif
#ifndef M_LN2
#define M_LN2      0.69314718055994530942
#endif
#ifndef M_LN10
#define M_LN10     2.30258509299404568402
#endif
#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2     1.57079632679489661923
#endif
#ifndef M_PI_4
#define M_PI_4     0.78539816339744830962
#endif
#ifndef M_1_PI
#define M_1_PI     0.31830988618379067154
#endif
#ifndef M_2_PI
#define M_2_PI     0.63661977236758134308
#endif
#ifndef M_2_SQRTPI
#define M_2_SQRTPI 1.12837916709551257390
#endif
#ifndef M_SQRT2
#define M_SQRT2    1.41421356237309504880
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2  0.70710678118654752440
#endif

double sin(double);
double cos(double);
double tan(double);
double asin(double);
double acos(double);
double atan(double);
double atan2(double, double);

double sinh(double);
double cosh(double);
double tanh(double);

double exp(double);
double exp2(double);
double log(double);
double log2(double);
double log10(double);

double pow(double, double);
double sqrt(double);
double cbrt(double);
double hypot(double, double);

double ceil(double);
double floor(double);
double trunc(double);
double round(double);
double fabs(double);
double fmod(double, double);

float sinf(float);
float cosf(float);
float tanf(float);
float sqrtf(float);
float fabsf(float);
float powf(float, float);

#endif
