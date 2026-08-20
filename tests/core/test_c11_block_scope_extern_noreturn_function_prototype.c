void
f(void)
{
}

int
main(void)
{
	extern _Noreturn void f(void);
	f();
	return 42;
}
