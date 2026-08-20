#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

static _Complex double
complex_double_id(_Complex double value)
{
	return value;
}

int
main(void)
{
	_Complex double value = (_Complex double)5.0;
	_Complex double result;
	double *parts;

	((double *)&value)[1] = 6.0;
	result = complex_double_id(value);
	parts = (double *)&result;
	if (parts[0] != 5.0)
		return 1;
	if (parts[1] != 6.0)
		return 2;
	return 42;
}
