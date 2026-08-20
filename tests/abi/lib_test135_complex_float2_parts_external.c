#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

_Complex float
make_complex_float_parts(float real, float imag)
{
	_Complex float value = (_Complex float)real;

	((float *)&value)[1] = imag;
	return value;
}

int
consume_complex_float_parts(_Complex float value)
{
	float *parts = (float *)&value;

	if (parts[0] != 3.0f)
		return 1;
	if (parts[1] != 4.0f)
		return 2;
	return 42;
}
