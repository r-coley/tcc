int
main(void)
{
	const volatile double d = 0.0;

	return _Generic(d, double: 1, const volatile double: 2, default: 3);
}
