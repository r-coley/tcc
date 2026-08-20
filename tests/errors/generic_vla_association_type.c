int
main(void)
{
	int n = 2;
	int a[2];
	return _Generic(a, int[n]: 1, int *: 2, default: 3);
}
