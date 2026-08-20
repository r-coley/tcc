#include <stddef.h>

static _Complex long double
imag_to_complex_ret(_Imaginary long double x)
{
	return x;
}

int
main(void)
{
	_Complex long double (*fp)(_Imaginary long double);
	_Imaginary double z = (_Imaginary double)1.0;
	_Imaginary double w = (_Imaginary double){ 2.0 };
	_Imaginary float ifv = (_Imaginary float)1.5f;
	_Imaginary long double ild = (_Imaginary long double)2.0;
	_Imaginary double x;
	_Imaginary double *px = &x;
	double real;
	double *raw;
	long double lreal;
	long double *lraw;
	_Complex double c;
	double *parts;
	_Complex float cf;
	_Complex long double cld;
	long double *clparts;

	x = z;
	*px = w;
	x = z + w;
	raw = (double *)&x;
	if (*raw != 3.0)
		return 1;

	x = -x;
	if (*raw != -3.0)
		return 2;

	x += (_Imaginary double)4.0;
	if (*raw != 1.0)
		return 3;

	x = ifv;
	if (*raw != 1.5)
		return 4;

	x = ifv + z;
	if (*raw != 2.5)
		return 5;

	x = z - ifv;
	if (*raw != -0.5)
		return 6;

	c = ifv;
	parts = (double *)&c;
	if (parts[0] != 0.0 || parts[1] != 1.5)
		return 7;

	c = ifv * (_Imaginary double)2.5;
	if (parts[0] != -3.75 || parts[1] != 0.0)
		return 8;

	real = z;
	if (real != 0.0)
		return 9;

	c = 4.0 + (_Imaginary double)2.5;
	x = c;
	if (*raw != 2.5)
		return 10;

	x = (_Imaginary double)ifv;
	if (*raw != 1.5)
		return 11;

	lraw = (long double *)&ild;
	ild = (_Imaginary long double)x;
	if (*lraw != 1.5L)
		return 12;

	real = (double)x;
	if (real != 0.0)
		return 13;

	c = (_Complex double)x;
	if (parts[0] != 0.0 || parts[1] != 1.5)
		return 14;

	lreal = (long double)c;
	if (lreal != 0.0L)
		return 15;

	x = (_Imaginary double)c;
	if (*raw != 1.5)
		return 16;

	ild = (_Imaginary long double)2.0;
	x = (_Imaginary double)1.0;
	ild = ild + x;
	if (*lraw != 3.0L)
		return 17;

	cf = 1.0f + (_Imaginary float)2.0f;
	cld = cf + ild;
	clparts = (long double *)&cld;
	if (clparts[0] != 1.0L || clparts[1] != 5.0L)
		return 18;

	cld = ild + cf;
	if (clparts[0] != 1.0L || clparts[1] != 5.0L)
		return 19;

	cld = cf - ild;
	if (clparts[0] != 1.0L || clparts[1] != -1.0L)
		return 20;

	cld = (_Complex long double)cf;
	if (clparts[0] != 1.0L || clparts[1] != 2.0L)
		return 21;

	ild = (_Imaginary long double)cf;
	if (*lraw != 2.0L)
		return 22;

	cld = ifv / ild;
	if (clparts[0] != 0.75L || clparts[1] != 0.0L)
		return 23;

	cld = ild / ifv;
	if (clparts[0] != (4.0L / 3.0L) || clparts[1] != 0.0L)
		return 24;

	x = z * w;
	if (*raw != -2.0)
		return 25;

	if (((double)(z / w)) != 0.5)
		return 26;

	x = z * 3.0;
	if (*raw != 3.0)
		return 27;

	x = 4.0 * z;
	if (*raw != 4.0)
		return 28;

	x = w / 2.0;
	if (*raw != 1.0)
		return 29;

	x = 4.0 / w;
	if (*raw != -2.0)
		return 30;

	c = 4.0 + z;
	parts = (double *)&c;
	if (parts[0] != 4.0 || parts[1] != 1.0)
		return 31;

	c = 4.0 - z;
	if (parts[0] != 4.0 || parts[1] != -1.0)
		return 32;

	c = z - 4.0;
	if (parts[0] != -4.0 || parts[1] != 1.0)
		return 33;

	c = (4.0 + z) + 2.0;
	if (parts[0] != 6.0 || parts[1] != 1.0)
		return 34;

	c = 2.0 + (4.0 + z);
	if (parts[0] != 6.0 || parts[1] != 1.0)
		return 35;

	c = 2.0 - (4.0 + z);
	if (parts[0] != -2.0 || parts[1] != -1.0)
		return 36;

	c = (4.0 + z) * 2.0;
	if (parts[0] != 8.0 || parts[1] != 2.0)
		return 37;

	c = 2.0 * (4.0 + z);
	if (parts[0] != 8.0 || parts[1] != 2.0)
		return 38;

	c = (4.0 + z) / 2.0;
	if (parts[0] != 2.0 || parts[1] != 0.5)
		return 39;

	c = 2.0 / (4.0 + z);
	if (parts[0] != (8.0 / 17.0) || parts[1] != (-2.0 / 17.0))
		return 40;

	c = (4.0 + z) + (_Imaginary double)2.0;
	if (parts[0] != 4.0 || parts[1] != 3.0)
		return 41;

	c = (_Imaginary double)2.0 + (4.0 + z);
	if (parts[0] != 4.0 || parts[1] != 3.0)
		return 42;

	c = (_Imaginary double)2.0 - (4.0 + z);
	if (parts[0] != -4.0 || parts[1] != 1.0)
		return 43;

	c = (4.0 + z) * (_Imaginary double)2.0;
	if (parts[0] != -2.0 || parts[1] != 8.0)
		return 44;

	c = (_Imaginary double)2.0 * (4.0 + z);
	if (parts[0] != -2.0 || parts[1] != 8.0)
		return 45;

	c = (4.0 + z) / (_Imaginary double)2.0;
	if (parts[0] != 0.5 || parts[1] != -2.0)
		return 46;

	c = (_Imaginary double)2.0 / (4.0 + z);
	if (parts[0] != (2.0 / 17.0) || parts[1] != (8.0 / 17.0))
		return 47;

	c = 4.0 + z;
	c *= (_Imaginary double)2.0;
	if (parts[0] != -2.0 || parts[1] != 8.0)
		return 48;

	c = 4.0 + z;
	c /= (_Imaginary double)2.0;
	if (parts[0] != 0.5 || parts[1] != -2.0)
		return 49;

	c = 1.0 + (_Imaginary double)2.0;
	if (!(c == (_Imaginary double)2.0))
		return 50;
	if (c != (_Imaginary double)2.0)
		return 51;
	if (!((_Imaginary double)2.0 == c))
		return 52;
	if ((_Imaginary double)2.0 != c)
		return 53;

	if (sizeof z != sizeof(double))
		return 54;
	if (sizeof((_Imaginary float){ 3.0f }) != sizeof(float))
		return 55;
	if (sizeof *px != sizeof(double))
		return 56;

	cld = imag_to_complex_ret((_Imaginary long double)3.0L);
	if (clparts[0] != 0.0L || clparts[1] != 3.0L)
		return 57;

	fp = imag_to_complex_ret;
	cld = fp((_Imaginary long double)4.0L);
	if (clparts[0] != 0.0L || clparts[1] != 4.0L)
		return 58;

	return 59;
}
