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
call(variadic_outer fn)
{
	return fn()(42, 7);
}

int
main(void)
{
	return call(make_target) - 42;
}
