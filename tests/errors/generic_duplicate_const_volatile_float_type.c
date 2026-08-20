int
main(void)
{
	const volatile float f = 0.0f;

	return _Generic(f, float: 1, const volatile float: 2, default: 3);
}
