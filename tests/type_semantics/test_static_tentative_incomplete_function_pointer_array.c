static int
forty_two(void)
{
	return 42;
}

static int (*a[])(void);

int
main(void)
{
	if (a[0] != 0)
		return 1;
	a[0] = forty_two;
	return a[0]();
}
