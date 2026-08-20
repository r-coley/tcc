void
f(void)
{
}

int
main(void)
{
	extern void _Noreturn f(void);
	f();
	return 42;
}
