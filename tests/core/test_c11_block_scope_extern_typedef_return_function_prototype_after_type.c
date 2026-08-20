typedef int myint;

myint
f(void)
{
	return 42;
}

int
main(void)
{
	myint extern f(void);
	return f();
}
