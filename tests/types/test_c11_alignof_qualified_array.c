int
main(void)
{
	const int a[3] = { 1, 2, 3 };
	volatile int b[3];

	if (_Alignof(int[3]) != _Alignof(int))
		return 1;
	if (_Alignof(const int[3]) != _Alignof(int[3]))
		return 2;
	if (_Alignof(volatile int[3]) != _Alignof(int[3]))
		return 3;
	if (_Alignof(a) != _Alignof(int[3]))
		return 4;
	if (_Alignof(b) != _Alignof(int[3]))
		return 5;

	return 42;
}
