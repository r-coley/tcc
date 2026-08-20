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

int static (*values[2])(void) = { forty, two };
int extern (*values[2])(void);

int
main(void)
{
	return values[0]() + values[1]();
}
