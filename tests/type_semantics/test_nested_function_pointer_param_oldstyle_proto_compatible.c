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
call(old_fn_ptr pp);

int
call(new_fn_ptr pp);

int
call(old_fn_ptr pp)
{
	return (*pp)(9);
}

int
main(void)
{
	new_fn p = target;

	return call(&p) - 9;
}
