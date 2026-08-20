#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

static _Complex double
mk_complex(double real, double imag)
{
	_Complex double value = (_Complex double)real;

	((double *)&value)[1] = imag;
	return value;
}

static int
check_nested_complex_args(_Complex double first, long middle, _Complex double second)
{
	double *a = (double *)&first;
	double *b = (double *)&second;

	if (a[0] != 2.0)
		return 1;
	if (a[1] != 3.0)
		return 2;
	if (middle != 4)
		return 3;
	if (b[0] != 5.0)
		return 4;
	if (b[1] != 6.0)
		return 5;
	return 42;
}

int
main(void)
{
	return check_nested_complex_args(mk_complex(2.0, 3.0), 4,
	                                 mk_complex(5.0, 6.0));
}
