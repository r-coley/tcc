typedef int (*plain_inner)(int);
typedef plain_inner (*plain_outer)(void);

extern plain_outer gp;

typedef int (*variadic_inner)(int, ...);
typedef variadic_inner (*variadic_outer)(void);

extern variadic_outer gp;

int
main(void)
{
	return 0;
}
