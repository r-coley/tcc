int
main(void)
{
	int * const p = 0;
	int * volatile q = 0;

	if (_Alignof(int *) != _Alignof(void *))
		return 1;
	if (_Alignof(int * const) != _Alignof(int *))
		return 2;
	if (_Alignof(int * volatile) != _Alignof(int *))
		return 3;
	if (_Alignof(p) != _Alignof(int *))
		return 4;
	if (_Alignof(q) != _Alignof(int *))
		return 5;

	return 42;
}
