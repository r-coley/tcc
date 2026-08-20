inline int
f(void)
{
	return 42;
}

int
main(void)
{
	return f() == 42 ? 42 : 1;
}
