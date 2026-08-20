typedef const int *cip;

int
main(void)
{
	int x = 0;
	const int *p = &x;
	return _Generic(p, const int *: 1, cip: 2, default: 3);
}
