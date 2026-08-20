int
main(void)
{
	int x = 0;
	const void * const cp = &x;
	volatile void * const vp = &x;
	const volatile void * const cvp = &x;
	int total = 0;

	total += _Generic(cp, const void *: 10, default: 100);
	total += _Generic(vp, volatile void *: 20, default: 100);
	total += _Generic(cvp, const volatile void *: 30, default: 100);

	return total == 60 ? 42 : total;
}
