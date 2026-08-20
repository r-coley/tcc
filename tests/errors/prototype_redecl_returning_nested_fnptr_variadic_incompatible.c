typedef int (*plain_inner)(int);
typedef plain_inner (*plain_outer)(void);

plain_outer
maker(void);

typedef int (*variadic_inner)(int, ...);
typedef variadic_inner (*variadic_outer)(void);

variadic_outer
maker(void);

int
main(void)
{
	return 0;
}
