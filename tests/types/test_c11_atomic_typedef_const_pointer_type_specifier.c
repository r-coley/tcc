typedef const int *ciptr;

int
main(void)
{
	int x = 7;
	int y = 11;
	_Atomic(ciptr) p = &x;
	_Atomic(ciptr) q = &y;

	if (*p != 7)
		return 1;

	p = q;
	if (*p != 11)
		return 2;

	return 42;
}
