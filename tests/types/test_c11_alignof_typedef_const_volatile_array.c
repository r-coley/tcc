typedef const volatile int CVA3[3];

int
main(void)
{
	CVA3 *p = 0;

	if (_Alignof(CVA3) != _Alignof(int))
		return 1;
	if (_Alignof(*p) != _Alignof(int))
		return 2;

	return 42;
}
