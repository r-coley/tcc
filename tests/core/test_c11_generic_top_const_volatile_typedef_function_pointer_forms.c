typedef int (*fn_t)(int);
typedef fn_t *fnpp_t;
typedef fn_t fnarr_t[2];

static int
f(int x)
{
	return x;
}

int
main(void)
{
	fn_t p = f;
	fn_t const volatile fp = f;
	fnpp_t const volatile pp = &p;
	fnarr_t arr = {f, f};
	fnarr_t * const volatile ap = &arr;
	int total = 0;

	total += _Generic(fp, fn_t: 10, default: 100);
	total += _Generic(fp, int (*)(int): 20, default: 100);
	total += _Generic(pp, fnpp_t: 30, default: 100);
	total += _Generic(pp, int (**)(int): 40, default: 100);
	total += _Generic(ap, fnarr_t *: 50, default: 100);
	total += _Generic(ap, int (*(*)[2])(int): 60, default: 100);

	return total == 210 ? 42 : total;
}
