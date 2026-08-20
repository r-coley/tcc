typedef int (*plain_fn)(int);
typedef plain_fn *plain_ptr;
typedef int (*variadic_fn)(int, ...);
typedef variadic_fn *variadic_ptr;

int
call(variadic_ptr pp)
{
	return (*pp)(42, 7);
}

int
target(int x)
{
	return x;
}

int
main(void)
{
	plain_fn p = target;

	return call(&p);
}
