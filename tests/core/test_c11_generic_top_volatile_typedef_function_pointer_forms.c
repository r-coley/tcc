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
	volatile fn_t fpv = f;
	const volatile fn_t fpcv = f;
	volatile fnpp_t ppv = &p;
	const volatile fnpp_t ppcv = &p;
	fnarr_t arr = {f, f};
	volatile fnarr_t *apv = &arr;
	const volatile fnarr_t *apcv = &arr;
	int total = 0;

	total += _Generic(fpv, fn_t: 10, default: 100);
	total += _Generic(fpv, int (*)(int): 20, default: 100);
	total += _Generic(fpcv, fn_t: 30, default: 100);
	total += _Generic(fpcv, int (*)(int): 40, default: 100);
	total += _Generic(ppv, fnpp_t: 50, default: 100);
	total += _Generic(ppv, int (**)(int): 60, default: 100);
	total += _Generic(ppcv, fnpp_t: 70, default: 100);
	total += _Generic(ppcv, int (**)(int): 80, default: 100);
	/* `volatile fnarr_t *` points at a volatile-qualified array type, so it
	 * does not match an association written as an unqualified `fnarr_t *`. */
	total += _Generic(apv, fnarr_t *: 90, default: 100);
	total += _Generic(apv, int (*(*)[2])(int): 91, default: 100);
	total += _Generic(apcv, fnarr_t *: 92, default: 100);
	total += _Generic(apcv, int (*(*)[2])(int): 93, default: 100);

	return total == 760 ? 42 : total;
}
