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
extern int (*values[2])(void);

int
main(void)
{
	return values[0]() + values[1]();
}
