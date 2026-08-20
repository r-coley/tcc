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
	int (* volatile fpv)(int) = f;
	int (* const volatile fpcv)(int) = f;
	fnpp_t volatile ppv = &p;
	fnpp_t const volatile ppcv = &p;
	int total = 0;

	total += _Generic(fpv, int (*)(int): 10, default: 100);
	total += _Generic(fpcv, int (*)(int): 20, default: 100);
	total += _Generic(ppv, int (**)(int): 30, default: 100);
	total += _Generic(ppcv, int (**)(int): 40, default: 100);

	return total == 100 ? 42 : total;
}
