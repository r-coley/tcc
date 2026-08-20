int
main(void)
{
	int x = 0;
	int *p = &x;
	int **pp = &p;
	return _Generic(pp, int ** const: 1, int **: 2, default: 3);
}
