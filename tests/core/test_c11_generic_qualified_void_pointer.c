int
main(void)
{
	int x = 0;
	volatile void *vp = &x;
	const volatile void *cvp = &x;
	int total = 0;

	total += _Generic(vp, volatile void *: 10, void *: 1, default: 100);
	total += _Generic(cvp, const volatile void *: 20, const void *: 2,
		volatile void *: 3, default: 100);

	return total == 30 ? 42 : total;
}
