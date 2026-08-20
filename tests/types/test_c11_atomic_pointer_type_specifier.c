int
main(void)
{
	int x = 11;
	int y = 23;
	_Atomic(int *) p = &x;
	_Atomic(int *) q = &y;
	_Atomic(int *) *pp = &p;

	if (*p != 11)
		return 1;

	p = q;
	if (*p != 23)
		return 2;

	*pp = &x;
	if (*p != 11)
		return 3;

	return 42;
}
