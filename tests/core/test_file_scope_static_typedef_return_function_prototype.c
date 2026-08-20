typedef int myint;

static myint forty_two(void);

static myint
forty_two(void)
{
	return 42;
}

int
main(void)
{
	return forty_two();
}
