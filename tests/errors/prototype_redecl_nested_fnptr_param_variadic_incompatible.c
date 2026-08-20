typedef int (*plain_fn)(int);
typedef plain_fn *plain_ptr;

int
call(plain_ptr p);

typedef int (*variadic_fn)(int, ...);
typedef variadic_fn *variadic_ptr;

int
call(variadic_ptr p);

int
main(void)
{
	return 0;
}
