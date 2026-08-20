#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

static int
consume_complex_double_indirect(long left, _Complex double value, long right)
{
	double *parts = (double *)&value;

	if (left != 5)
		return 1;
	if (parts[0] != 7.0)
		return 2;
	if (parts[1] != 8.0)
		return 3;
	if (right != 9)
		return 4;
	return 42;
}

int
main(void)
{
	int (*fp)(long, _Complex double, long) = consume_complex_double_indirect;
	_Complex double value = (_Complex double)7.0;

	((double *)&value)[1] = 8.0;
	return fp(5, value, 9);
}
