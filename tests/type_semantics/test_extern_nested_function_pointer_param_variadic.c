typedef int (*variadic_fn)(int, ...);
typedef variadic_fn *variadic_ptr;

int
target(int x, ...)
{
	return x;
}

variadic_fn gp_impl = target;
extern variadic_ptr gp;
variadic_ptr gp = &gp_impl;

int
main(void)
{
	return (*gp)(42, 5) - 42;
}
