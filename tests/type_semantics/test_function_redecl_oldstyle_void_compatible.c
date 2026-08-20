int f();
int f(void);

int
f(void)
{
	return 42;
}

int
main(void)
{
	return f() - 42;
}
