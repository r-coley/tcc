int
main(void)
{
	volatile int nonzero = -7;
	volatile int zero = 0;
	_Bool b;

	if ((_Bool)0 != 0)
		return 1;
	if ((_Bool)2 != 1)
		return 2;
	if ((_Bool)-3 != 1)
		return 3;
	if ((_Bool)nonzero != 1)
		return 9;
	if ((_Bool)zero != 0)
		return 10;

	b = nonzero;
	if (b != 1)
		return 4;
	b = zero;
	if (b != 0)
		return 5;

	if ((b + 1) != 1)
		return 6;
	b = 9;
	if ((b + 1) != 2)
		return 7;
	if (sizeof(+b) != sizeof(int))
		return 8;

	return 42;
}
