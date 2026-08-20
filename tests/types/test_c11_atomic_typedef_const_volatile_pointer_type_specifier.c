typedef const volatile int *cviptr;

int
main(void)
{
	int x = 7;
	int y = 11;
	_Atomic(cviptr) p = &x;
	_Atomic(cviptr) q = &y;

	if (*p != 7)
		return 1;

	p = q;
	if (*p != 11)
		return 2;

	return 42;
}
