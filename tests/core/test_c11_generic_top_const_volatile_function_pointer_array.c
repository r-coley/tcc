typedef int (*fn_t)(int);
typedef fn_t fnarr_t[2];

static int
f(int x)
{
	return x;
}

static int
g(int x)
{
	return x + 1;
}

int
main(void)
{
	fnarr_t arr = {f, g};
	fnarr_t * const volatile ap = &arr;
	int total = 0;

	total += _Generic(ap, fnarr_t *: 10, default: 100);
	total += _Generic(ap, int (*(*)[2])(int): 20, default: 100);

	return total == 30 ? 42 : total;
}
