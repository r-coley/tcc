typedef volatile restrict int VRA3[3];

int
main(void)
{
	VRA3 *p = 0;

	if (_Alignof(VRA3) != _Alignof(int))
		return 1;
	if (_Alignof(*p) != _Alignof(int))
		return 2;

	return 42;
}
