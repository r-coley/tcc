typedef int (*variadic_fn)(int, ...);
typedef variadic_fn *variadic_ptr;

int
target(int x, ...)
{
	return x;
}

int
main(void)
{
	variadic_fn p = target;
	variadic_ptr a = &p;
	variadic_ptr b = &p;

	return (a == b) ? 0 : 1;
}
