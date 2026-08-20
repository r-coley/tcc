typedef int myint;

myint forty(void), two(void);

myint
forty(void)
{
	return 40;
}

myint
two(void)
{
	return 2;
}

int
main(void)
{
	return forty() + two();
}
