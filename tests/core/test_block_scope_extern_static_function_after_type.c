int static
value(void)
{
	return 40;
}

int
main(void)
{
	int extern value(void);
	return value() + 2;
}
