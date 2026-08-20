typedef int (*variadic_inner)(int, ...);
typedef variadic_inner (*variadic_outer)(void);

int
inc(int x, ...)
{
	return x + 1;
}

int
dec(int x, ...)
{
	return x - 1;
}

variadic_inner
make_inc(void)
{
	return inc;
}

variadic_inner
make_dec(void)
{
	return dec;
}

int
main(void)
{
	static variadic_outer fn;

	fn = 1 ? make_inc : make_dec;
	return fn()(41, 7) - 42;
}
