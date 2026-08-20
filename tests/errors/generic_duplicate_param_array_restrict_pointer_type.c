static int
check(int a[restrict 3])
{
	return _Generic(a, int *: 1, int *restrict: 2, default: 3);
}

int
main(void)
{
	int a[3] = {1, 2, 3};
	return check(a);
}
