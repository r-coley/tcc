int
main(void)
{
	int n = 3;
	int a = _Alignof(const int[n]);
	int b = _Alignof(volatile int[++n]);

	if (a != _Alignof(int))
		return 1;
	if (b != _Alignof(int))
		return 2;
	if (n != 3)
		return 3;

	return 42;
}
