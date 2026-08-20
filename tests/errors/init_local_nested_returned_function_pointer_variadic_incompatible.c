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

int
main(void)
{
	plain_outer a = make_target;
	variadic_outer b = a;

	return 0;
}
