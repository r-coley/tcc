typedef int myint;

myint extern forty_two(void);

myint
forty_two(void)
{
	return 42;
}

int
main(void)
{
	return forty_two();
}
