#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

_Complex double
make_complex_double(double real)
{
	return (_Complex double)real;
}

int
consume_complex_double(_Complex double value)
{
	if ((double)value != 24.0)
		return 1;
	return 42;
}
