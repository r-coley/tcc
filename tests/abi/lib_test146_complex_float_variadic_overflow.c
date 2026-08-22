extern int check_complex_float_variadic_overflow(float, float, float, float,
                                                 float, float, float, float,
                                                 ...);

int
main(void)
{
	_Complex float value;
	float *parts = (float *)&value;

	parts[0] = 11.5f;
	parts[1] = 12.5f;
	return check_complex_float_variadic_overflow(1.0f, 2.0f, 3.0f, 4.0f,
	                                             5.0f, 6.0f, 7.0f, 8.0f, value);
}
