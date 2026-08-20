#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

static _Complex float
complex_float_id(_Complex float value)
{
	return value;
}

int
main(void)
{
	_Complex float value = (_Complex float)3.0f;
	_Complex float result;
	float *parts;

	((float *)&value)[1] = 4.0f;
	result = complex_float_id(value);
	parts = (float *)&result;
	if (parts[0] != 3.0f)
		return 1;
	if (parts[1] != 4.0f)
		return 2;
	return 42;
}
