int
target(int x)
{
	return x + 1;
}

typedef int (*fn_t)(int);

int
invoke(int (**pp)(int), int x)
{
	return (*pp)(x);
}

int
main(void)
{
	fn_t p = target;

	return invoke(&p, 41) - 42;
}
