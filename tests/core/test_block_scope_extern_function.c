int
f(void)
{
	return 42;
}

int
main(void)
{
	extern int f(void);
	return f();
}
