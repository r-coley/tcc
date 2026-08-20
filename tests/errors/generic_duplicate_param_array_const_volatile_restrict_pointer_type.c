static int
check(const volatile int a[const volatile restrict 3])
{
	return _Generic(a,
	                const volatile int *: 1,
	                const volatile int *const volatile restrict: 2,
	                default: 3);
}

int
main(void)
{
	const volatile int a[3] = {1, 2, 3};
	return check(a);
}
