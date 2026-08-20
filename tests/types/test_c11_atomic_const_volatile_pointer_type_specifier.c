int
main(void)
{
	int x = 5;
	int y = 9;
	_Atomic(const volatile int *) p = &x;
	_Atomic(const volatile int *) q = &y;

	if (*p != 5)
		return 1;

	p = q;
	if (*p != 9)
		return 2;

	return 42;
}
