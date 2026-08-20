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
	fnarr_t * const volatile p = &arr;
	return _Generic(p, fnarr_t *: 1, int (* const volatile (*)[2])(int): 2, default: 3);
}
