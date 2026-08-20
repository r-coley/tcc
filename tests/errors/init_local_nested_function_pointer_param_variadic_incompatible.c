typedef int (*plain_fn)(int);
typedef int (*variadic_fn)(int, ...);
typedef plain_fn *plain_ptr;
typedef variadic_fn *variadic_ptr;

int
target(int x)
{
	return x;
}

int
main(void)
{
	plain_fn p = target;
	plain_ptr a = &p;
	variadic_ptr b = a;

	return 0;
}
