int
f(void)
{
	return 42;
}

int
main(void)
{
	int inline f(void);
	return f();
}
