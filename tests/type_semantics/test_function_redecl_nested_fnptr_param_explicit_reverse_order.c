typedef int (*step_fn)(int);
typedef step_fn *chooser_ptr;

int
call(int (*(*pp))(int));

int
call(chooser_ptr pp);

int
target(int x)
{
	return x + 1;
}

int
call(chooser_ptr pp)
{
	return (*pp)(4);
}

int
main(void)
{
	step_fn p = target;

	return call(&p) - 5;
}
