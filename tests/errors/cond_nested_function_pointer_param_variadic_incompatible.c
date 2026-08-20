typedef int (*plain_fn)(int);
typedef int (*variadic_fn)(int, ...);
typedef plain_fn *plain_ptr;
typedef variadic_fn *variadic_ptr;

int
target_plain(int x)
{
	return x;
}

int
target_var(int x, ...)
{
	return x;
}

int
main(void)
{
	plain_fn p = target_plain;
	variadic_fn v = target_var;

	return (1 ? &p : &v) != 0;
}
