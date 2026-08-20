typedef int (*fn_t)(int);
typedef fn_t fnarr_t[2];

#define MATCH_FN_PTR(x) _Generic((x), fn_t: 1, default: 99)
#define MATCH_DIRECT_FN_PTR(x) _Generic((x), int (*)(int): 2, default: 99)
#define MATCH_FN_PTR_PTR(x) _Generic((x), fn_t *: 3, default: 99)
#define MATCH_DIRECT_FN_PTR_PTR(x) _Generic((x), int (**)(int): 4, default: 99)
#define MATCH_FNARR_PTR(x) _Generic((x), fnarr_t *: 5, default: 99)
#define MATCH_DIRECT_FNARR_PTR(x) _Generic((x), int (*(*)[2])(int): 6, default: 99)

static int
inc(int x)
{
	return x + 1;
}

static int
mul2(int x)
{
	return x * 2;
}

int
main(void)
{
	fn_t pf = inc;
	fn_t *ppf = &pf;
	fnarr_t arr = {inc, mul2};

	if (MATCH_FN_PTR(inc) != 1)
		return 1;
	if (MATCH_DIRECT_FN_PTR(inc) != 2)
		return 2;
	if (MATCH_FN_PTR(pf) != 1)
		return 3;
	if (MATCH_DIRECT_FN_PTR(pf) != 2)
		return 4;
	if (MATCH_FN_PTR_PTR(ppf) != 3)
		return 5;
	if (MATCH_DIRECT_FN_PTR_PTR(ppf) != 4)
		return 6;
	if (MATCH_FNARR_PTR(&arr) != 5)
		return 7;
	if (MATCH_DIRECT_FNARR_PTR(&arr) != 6)
		return 8;
	if (MATCH_FN_PTR(arr[0]) != 1)
		return 9;
	if (MATCH_DIRECT_FN_PTR(arr[1]) != 2)
		return 10;
	if (arr[0](41) != 42)
		return 11;
	if (arr[1](21) != 42)
		return 12;

	return 42;
}
