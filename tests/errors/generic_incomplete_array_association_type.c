int
main(void)
{
	int a[2];
	return _Generic(a, int[]: 1, int *: 2, default: 3);
}
