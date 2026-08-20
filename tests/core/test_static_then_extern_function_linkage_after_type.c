int static
value(void)
{
	return 40;
}

int extern value(void);

int
main(void)
{
	return value() + 2;
}
