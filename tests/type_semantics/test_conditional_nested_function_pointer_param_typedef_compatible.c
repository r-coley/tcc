typedef int (*old_fn)();
typedef int (*new_fn)(int);
typedef old_fn *old_fn_ptr;
typedef new_fn *new_fn_ptr;

int
inc(int x)
{
	return x + 1;
}

int
dec(int x)
{
	return x - 1;
}

int
main(void)
{
	new_fn a = inc;
	old_fn b = dec;
	new_fn_ptr fp = 1 ? &a : &b;

	return (*fp)(41) - 42;
}
