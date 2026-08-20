typedef int * const volatile restrict CVRIP;

int
main(void)
{
	CVRIP p = 0;

	if (_Alignof(CVRIP) != _Alignof(int *))
		return 1;
	if (_Alignof(p) != _Alignof(int *))
		return 2;

	return 42;
}
