int
main(void)
{
	int x = 0;
	const int *p = &x;
	const int **pp = &p;
	return _Generic(pp, const int ** const: 1, const int **: 2, default: 3);
}
