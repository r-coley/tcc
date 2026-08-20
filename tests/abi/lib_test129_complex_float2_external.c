#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

_Complex float
make_complex_float(float real)
{
	return (_Complex float)real;
}

int
consume_complex_float(_Complex float value)
{
	if ((float)value != 19.0f)
		return 1;
	return 42;
}
