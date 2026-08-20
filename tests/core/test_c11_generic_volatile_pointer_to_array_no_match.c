typedef int (*fn_t)(int);
typedef fn_t fnarr_t[2];

static int
f(int x)
{
	return x;
}

int
main(void)
{
	fnarr_t arr = {f, f};
	volatile fnarr_t *apv = &arr;
	const volatile fnarr_t *apcv = &arr;
	int total = 0;

	total += _Generic(apv, fnarr_t *: 10, default: 100);
	total += _Generic(apv, int (*(*)[2])(int): 20, default: 100);
	total += _Generic(apcv, fnarr_t *: 30, default: 100);
	total += _Generic(apcv, int (*(*)[2])(int): 40, default: 100);

	return total == 400 ? 42 : total;
}
