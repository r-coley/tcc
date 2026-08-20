typedef int (*variadic_fn)(int, ...);
typedef variadic_fn *variadic_ptr;

int
call(variadic_ptr pp);

int
call(variadic_ptr pp)
{
	return (*pp)(42, 100) - 42;
}

int
target(int x, ...)
{
	return x;
}

int
main(void)
{
	variadic_fn p = target;

	return call(&p);
}
