#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

static int
check_complex_double_multi(_Complex double first, _Complex double second)
{
	double *a = (double *)&first;
	double *b = (double *)&second;

	if (a[0] != 3.0)
		return 1;
	if (a[1] != 4.0)
		return 2;
	if (b[0] != 5.0)
		return 3;
	if (b[1] != 6.0)
		return 4;
	return 42;
}

int
main(void)
{
	_Complex double first = (_Complex double)3.0;
	_Complex double second = (_Complex double)5.0;

	((double *)&first)[1] = 4.0;
	((double *)&second)[1] = 6.0;
	return check_complex_double_multi(first, second);
}
