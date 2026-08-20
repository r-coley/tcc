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
	fnarr_t * const p = &arr;
	int total = 0;

	total += _Generic(p, fnarr_t *: 10, default: 100);
	total += _Generic(p, int (*(*)[2])(int): 20, default: 100);

	return total == 30 ? 42 : total;
}
