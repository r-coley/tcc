#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

_Complex double
make_complex_double_parts(double real, double imag)
{
	_Complex double value = (_Complex double)real;

	((double *)&value)[1] = imag;
	return value;
}

int
consume_complex_double_parts(_Complex double value)
{
	double *parts = (double *)&value;

	if (parts[0] != 5.0)
		return 1;
	if (parts[1] != 6.0)
		return 2;
	return 42;
}
