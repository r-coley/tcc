typedef int myint;

myint
f(void)
{
	return 42;
}

int
main(void)
{
	extern myint f(void);
	return f();
}
