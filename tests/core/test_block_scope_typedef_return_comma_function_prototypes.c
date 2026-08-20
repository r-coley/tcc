typedef int myint;

myint forty(void)
{
	return 40;
}

myint two(void)
{
	return 2;
}

int
main(void)
{
	myint forty(void), two(void);
	return forty() + two();
}
