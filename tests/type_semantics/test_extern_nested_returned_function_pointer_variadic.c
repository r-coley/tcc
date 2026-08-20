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

extern variadic_outer gp;
variadic_outer gp = make_target;

int
main(void)
{
	return gp()(42, 5) - 42;
}
