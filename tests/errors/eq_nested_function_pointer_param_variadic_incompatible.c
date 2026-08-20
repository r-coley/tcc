typedef int (*plain_fn)(int);
typedef int (*variadic_fn)(int, ...);
typedef plain_fn *plain_ptr;
typedef variadic_fn *variadic_ptr;

int
plain(int x)
{
	return x;
}

int
var(int x, ...)
{
	return x;
}

int
main(void)
{
	plain_fn p = plain;
	variadic_fn v = var;
	plain_ptr a = &p;
	variadic_ptr b = &v;

	return (a == b) ? 1 : 0;
}
