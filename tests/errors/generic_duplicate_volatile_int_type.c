int
main(void)
{
	volatile int vi = 0;

	return _Generic(vi, int: 1, volatile int: 2, default: 3);
}
