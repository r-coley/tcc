typedef const long *clp;

int
main(void)
{
	long x = 0;
	const long *p = &x;
	return _Generic(p, const long *: 1, clp: 2, default: 3);
}
