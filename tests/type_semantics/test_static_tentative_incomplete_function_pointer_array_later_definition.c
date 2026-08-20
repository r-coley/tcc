static int
forty_two(void)
{
	return 42;
}

static int (*a[])(void);
static int (*a[2])(void);

int
main(void)
{
	if (a[0] != 0 || a[1] != 0)
		return 1;
	a[1] = forty_two;
	return a[1]();
}
