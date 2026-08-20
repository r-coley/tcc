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
	variadic_outer a = make_target;
	variadic_outer b = a;

	return b()(42, 9) - 42;
}
