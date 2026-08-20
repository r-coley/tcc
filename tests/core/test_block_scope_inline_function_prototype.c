int
f(void)
{
	return 42;
}

int
main(void)
{
	inline int f(void);
	return f();
}
