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
	return _Generic(p, int (* const)(int): 1, fn_t: 2, default: 3);
}
