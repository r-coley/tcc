int
main(void)
{
	long double a = 1.25L;
	long double b = .5L;
	long double c = 0x1.8p1L;

	if (sizeof(a) != sizeof(double))
		return 1;
	if (!(a > 1.0 && a < 2.0))
		return 2;
	if (!(b > 0.0 && b < 1.0))
		return 3;
	if (!(c > 2.0 && c < 4.0))
		return 4;
	return 42;
}
