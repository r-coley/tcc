#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

static _Complex float
complex_float_id(_Complex float value)
{
	return value;
}

static int
complex_float_ok(_Complex float value)
{
	return (float)value == 19.0f;
}

int
main(void)
{
	_Complex float value = (_Complex float)19.0f;

	if (!complex_float_ok(complex_float_id(value)))
		return 1;
	return 42;
}
