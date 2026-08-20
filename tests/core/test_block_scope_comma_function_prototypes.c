int forty(void)
{
	return 40;
}

int two(void)
{
	return 2;
}

int
main(void)
{
	int forty(void), two(void);
	return forty() + two();
}
