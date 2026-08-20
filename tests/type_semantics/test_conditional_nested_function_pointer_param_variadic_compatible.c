typedef int (*variadic_fn)(int, ...);
typedef variadic_fn *variadic_ptr;

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

int
main(void)
{
	variadic_fn a = inc;
	variadic_fn b = dec;
	variadic_ptr fp = 1 ? &a : &b;

	return (*fp)(41, 7) - 42;
}
