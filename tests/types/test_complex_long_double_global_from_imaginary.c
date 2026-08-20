_Complex long double z = (_Imaginary long double)3.5L;

int
main(void)
{
	long double parts[2];

	parts[0] = ((long double *)&z)[0];
	parts[1] = ((long double *)&z)[1];

	if (parts[0] != 0.0L)
		return 1;

	if (parts[1] != 3.5L)
		return 2;

	return 42;
}
