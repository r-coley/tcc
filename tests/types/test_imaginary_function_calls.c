#include <stddef.h>

static _Imaginary double
f(_Imaginary double x)
{
	return x;
}

static _Imaginary double g(_Imaginary double x);

static _Imaginary double
g(_Imaginary double x)
{
	return x;
}

int
main(void)
{
	_Imaginary double z = (_Imaginary double)3.0;
	_Imaginary double (*fp)(_Imaginary double) = f;
	double *raw = (double *)&z;

	z = fp(z);
	if (*raw != 3.0)
		return 1;

	z = f((_Imaginary double)5.0);
	if (*raw != 5.0)
		return 2;

	z = g((_Imaginary double)7.0);
	if (*raw != 7.0)
		return 3;

	return 42;
}
