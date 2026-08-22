extern int check_complex_double_variadic_overflow(double, double, double, double,
                                                  double, double, double, double,
                                                  ...);

int
main(void)
{
	_Complex double value;
	double *parts = (double *)&value;

	parts[0] = 9.5;
	parts[1] = 10.5;
	return check_complex_double_variadic_overflow(1.0, 2.0, 3.0, 4.0,
	                                              5.0, 6.0, 7.0, 8.0, value);
}
