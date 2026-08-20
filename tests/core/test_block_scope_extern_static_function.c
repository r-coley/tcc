static int
value(void)
{
	return 40;
}

int
main(void)
{
	extern int value(void);
	return value() + 2;
}
