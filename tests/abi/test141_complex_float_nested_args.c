#if __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

static _Complex float
mk_complex_float(float real, float imag)
{
	_Complex float value = (_Complex float)real;

	((float *)&value)[1] = imag;
	return value;
}

static int
check_nested_complex_float_args(_Complex float first, int middle, _Complex float second)
{
	float first_real = ((float *)&first)[0];
	float first_imag = ((float *)&first)[1];
	float second_real = ((float *)&second)[0];
	float second_imag = ((float *)&second)[1];

	if ((int)first_real != 5)
		return 1;
	if ((int)first_imag != 6)
		return 2;
	if (middle != 9)
		return 3;
	if ((int)second_real != 7)
		return 4;
	if ((int)second_imag != 8)
		return 5;
	return 42;
}

int
main(void)
{
	return check_nested_complex_float_args(mk_complex_float(5.0f, 6.0f),
	                                       9,
	                                       mk_complex_float(7.0f, 8.0f));
}
