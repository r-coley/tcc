typedef int (*plain_fn)(int);
typedef plain_fn *plain_ptr;

extern plain_ptr gp;

typedef int (*variadic_fn)(int, ...);
typedef variadic_fn *variadic_ptr;

extern variadic_ptr gp;

int
main(void)
{
	return 0;
}
