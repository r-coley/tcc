typedef int myint;

myint extern forty(void), two(void);

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
