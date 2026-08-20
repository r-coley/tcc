int
sum_static(int a[static 3])
{
	return a[0] + a[1] + a[2];
}

int
sum_qualified(int a[const volatile restrict 3])
{
	return a[0] + a[1] + a[2];
}

int
main(void)
{
	int a[3] = {10, 20, 12};

	if (sum_static(a) != 42)
		return 1;
	if (sum_qualified(a) != 42)
		return 2;
	return 42;
}
