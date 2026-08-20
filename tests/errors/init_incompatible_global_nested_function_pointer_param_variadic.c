typedef int (*plain_fn)(int);
typedef plain_fn *plain_ptr;
typedef int (*variadic_fn)(int, ...);
typedef variadic_fn *variadic_ptr;

int
target(int x)
{
	return x;
}

plain_fn gp_impl = target;
variadic_ptr bad = &gp_impl;

int
main(void)
{
	return 0;
}
