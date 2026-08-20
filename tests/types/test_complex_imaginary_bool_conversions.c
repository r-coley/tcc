#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error complex types require C99 or later
#endif

static _Complex double
make_complex_value(double real, double imaginary)
{
	_Complex double value = (_Complex double)real;
	double *parts = (double *)&value;

	parts[1] = imaginary;
	return value;
}

static _Bool
return_complex_bool(void)
{
	_Complex double value = make_complex_value(0.0, 3.0);

	return value;
}

static _Bool
return_imaginary_bool(void)
{
	return (_Imaginary double)4.0;
}

static int
accept_bool(_Bool value)
{
	return value ? 1 : 0;
}

int
main(void)
{
	_Complex double complex_value;
	_Imaginary double imaginary_value;
	_Bool value;

	complex_value = make_complex_value(0.0, 2.0);
	if (((double *)&complex_value)[0] != 0.0 ||
	    ((double *)&complex_value)[1] != 2.0)
		return 12;
	value = (_Bool)complex_value;
	if (!value)
		return 1;

	complex_value = make_complex_value(0.0, 0.0);
	value = (_Bool)complex_value;
	if (value)
		return 2;

	complex_value = make_complex_value(5.0, 0.0);
	value = complex_value;
	if (!value)
		return 3;

	complex_value = make_complex_value(0.0, 6.0);
	_Bool initialized = complex_value;
	if (!initialized)
		return 4;

	complex_value = make_complex_value(0.0, 7.0);
	if (!accept_bool(complex_value))
		return 5;

	if (!return_complex_bool())
		return 6;

	imaginary_value = (_Imaginary double)8.0;
	if (!(_Bool)imaginary_value)
		return 7;

	value = imaginary_value;
	if (!value)
		return 8;

	imaginary_value = (_Imaginary double)0.0;
	if ((_Bool)imaginary_value)
		return 9;

	if (!accept_bool((_Imaginary double)9.0))
		return 10;

	if (!return_imaginary_bool())
		return 11;

	if (!(_Bool)make_complex_value(0.0, 10.0))
		return 13;

	if (!accept_bool(make_complex_value(0.0, 11.0)))
		return 14;

	return 42;
}
