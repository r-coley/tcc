#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

static _Complex double
complex_double_id(_Complex double value)
{
	return value;
}

static int
complex_double_ok(_Complex double value)
{
	return (double)value == 24.0;
}

int
main(void)
{
	_Complex double value = (_Complex double)24.0;

	if (!complex_double_ok(complex_double_id(value)))
		return 1;
	return 42;
}
