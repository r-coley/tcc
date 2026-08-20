typedef int (*old_fn)();
typedef int (*new_fn)(int);
typedef old_fn *old_fn_ptr;
typedef new_fn *new_fn_ptr;

int
target(int x)
{
	return x;
}

int
main(void)
{
	new_fn f = target;
	old_fn_ptr a = &f;
	new_fn_ptr b = a;

	return (a == b) ? 0 : 1;
}
