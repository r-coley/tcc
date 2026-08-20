static int
forty(void)
{
	return 40;
}

static int
two(void)
{
	return 2;
}

extern int (*a[2])(void);
int (*a[])(void) = { forty, two };

int
main(void)
{
	return a[0]() + a[1]() - 42;
}
