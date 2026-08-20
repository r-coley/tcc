void
f(void)
{
}

int
main(void)
{
	void extern f(void);
	f();
	return 42;
}
