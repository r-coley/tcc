typedef int (*variadic_fn)(int, ...);
typedef variadic_fn *variadic_ptr;

int
target(int x, ...)
{
	return x;
}

variadic_fn gp_impl = target;
variadic_ptr ok = &gp_impl;

int
main(void)
{
	return (*ok)(42, 5) - 42;
}
