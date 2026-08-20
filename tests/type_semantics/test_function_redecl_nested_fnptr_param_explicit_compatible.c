typedef int (*new_fn)(int);

int
call(int (*(*pp))());

int
call(int (*(*pp))(int));

int
target(int x)
{
	return x;
}

int
call(int (*(*pp))())
{
	return (*pp)(7);
}

int
main(void)
{
	new_fn p = target;

	return call(&p) - 7;
}
