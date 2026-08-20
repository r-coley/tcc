typedef int (*variadic_inner)(int, ...);
typedef variadic_inner (*variadic_outer)(void);

int
target(int x, ...)
{
	return x;
}

variadic_inner
make_target(void)
{
	return target;
}

int
main(void)
{
	static variadic_outer a;
	static variadic_outer b;

	a = make_target;
	b = make_target;
	return (a == b) ? 0 : 1;
}
