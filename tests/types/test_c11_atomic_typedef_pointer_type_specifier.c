typedef int *iptr;

int
main(void)
{
	int x = 7;
	int y = 9;
	_Atomic(iptr) p = &x;
	_Atomic(iptr) q = &y;

	if (*p != 7)
		return 1;

	p = q;
	if (*p != 9)
		return 2;

	return 42;
}
