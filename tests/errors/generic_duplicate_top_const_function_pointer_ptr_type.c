typedef int (*fn_t)(int);

static int
f(int x)
{
	return x;
}

int
main(void)
{
	fn_t p = f;
	fn_t *pp = &p;
	return _Generic(pp, int (** const)(int): 1, fn_t *: 2, default: 3);
}
