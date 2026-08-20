_Imaginary long double y = (_Imaginary long double)3.5L;
long double x = (_Imaginary long double)2.0L;

int
main(void)
{
	if (x != 0.0L)
		return 1;

	if (*((long double *)&y) != 3.5L)
		return 2;

	return 42;
}
