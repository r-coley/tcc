typedef int (*plain_inner)(int);
typedef plain_inner (*plain_outer)(void);
typedef int (*variadic_inner)(int, ...);
typedef variadic_inner (*variadic_outer)(void);

int
target(int x)
{
	return x;
}

plain_inner
make_target(void)
{
	return target;
}

variadic_outer bad = make_target;

int
main(void)
{
	return 0;
}
