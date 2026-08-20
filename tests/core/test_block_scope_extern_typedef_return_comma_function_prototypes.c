typedef int myint;

myint forty_one(void)
{
	return 41;
}

myint one(void)
{
	return 1;
}

int
main(void)
{
	extern myint forty_one(void), one(void);
	return forty_one() + one();
}
