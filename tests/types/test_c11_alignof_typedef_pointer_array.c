typedef int *P3[3];

int
main(void)
{
	P3 *p = 0;

	if (_Alignof(P3) != _Alignof(int *))
		return 1;
	if (_Alignof(*p) != _Alignof(int *))
		return 2;

	return 42;
}
