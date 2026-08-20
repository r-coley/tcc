int forty_one(void)
{
	return 41;
}

int one(void)
{
	return 1;
}

int
main(void)
{
	int extern forty_one(void), one(void);
	return forty_one() + one();
}
