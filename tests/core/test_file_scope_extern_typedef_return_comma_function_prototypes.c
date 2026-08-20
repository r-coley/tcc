typedef int myint;

extern myint forty_one(void), one(void);

myint
forty_one(void)
{
	return 41;
}

myint
one(void)
{
	return 1;
}

int
main(void)
{
	return forty_one() + one();
}
