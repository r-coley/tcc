static int
value(void)
{
	return 40;
}

extern int value(void);

int
main(void)
{
	return value() + 2;
}
