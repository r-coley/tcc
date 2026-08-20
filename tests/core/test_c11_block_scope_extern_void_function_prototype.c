void
f(void)
{
}

int
main(void)
{
	extern void f(void);
	f();
	return 42;
}
