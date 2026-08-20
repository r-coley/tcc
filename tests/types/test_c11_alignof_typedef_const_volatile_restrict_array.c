typedef const volatile restrict int CVRA3[3];

int
main(void)
{
	CVRA3 *p = 0;

	if (_Alignof(CVRA3) != _Alignof(int))
		return 1;
	if (_Alignof(*p) != _Alignof(int))
		return 2;

	return 42;
}
