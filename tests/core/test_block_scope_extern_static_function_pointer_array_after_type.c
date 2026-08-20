int static
forty(void)
{
	return 40;
}

int static
two(void)
{
	return 2;
}

int static (*values[])(void) = { forty, two };

int
main(void)
{
	int extern (*values[])(void);
	return values[0]() + values[1]();
}
