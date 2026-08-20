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

variadic_outer ok = make_target;

int
main(void)
{
	return ok()(42, 5) - 42;
}
