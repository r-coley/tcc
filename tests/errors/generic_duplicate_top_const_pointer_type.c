int
main(void)
{
	int x = 0;
	int *p = &x;
	return _Generic(p, int *: 1, int * const: 2, default: 3);
}
