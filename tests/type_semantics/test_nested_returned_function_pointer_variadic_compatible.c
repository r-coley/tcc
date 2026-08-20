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
	variadic_outer fn = make_target;

	return fn()(42, 7) - 42;
}
