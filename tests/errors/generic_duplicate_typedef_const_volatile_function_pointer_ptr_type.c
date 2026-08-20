typedef int (*fn_t)(int);
typedef fn_t *fnpp_t;

static int
f(int x)
{
	return x;
}

int
main(void)
{
	fn_t p = f;
	const volatile fnpp_t pp = &p;
	return _Generic(pp, int (**)(int): 1, fnpp_t: 2, default: 3);
}
