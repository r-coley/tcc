typedef int * volatile restrict VRIP;

int
main(void)
{
	VRIP p = 0;

	if (_Alignof(VRIP) != _Alignof(int *))
		return 1;
	if (_Alignof(p) != _Alignof(int *))
		return 2;

	return 42;
}
