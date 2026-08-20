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

static int (*values[2])(void) = { forty, two };

int
main(void)
{
	extern int (*values[])(void);
	return values[0]() + values[1]();
}
