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
	myint extern forty_one(void), one(void);
	return forty_one() + one();
}
