int
main(void)
{
	int * const volatile restrict p = 0;

	if (_Alignof(int *) != _Alignof(void *))
		return 1;
	if (_Alignof(int * const volatile restrict) != _Alignof(int *))
		return 2;
	if (_Alignof(p) != _Alignof(int *))
		return 3;

	return 42;
}
