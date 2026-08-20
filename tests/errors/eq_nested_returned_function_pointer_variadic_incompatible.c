typedef int (*plain_inner)(int);
typedef plain_inner (*plain_outer)(void);
typedef int (*variadic_inner)(int, ...);
typedef variadic_inner (*variadic_outer)(void);

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

plain_inner
make_plain(void)
{
	return target_plain;
}

variadic_inner
make_var(void)
{
	return target_var;
}

int
main(void)
{
	return (make_plain == make_var) ? 1 : 0;
}
