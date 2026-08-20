typedef int (*vfn_t)(int, ...);
typedef vfn_t vfnarr_t[2];

static int
add1(int x, ...)
{
	return x + 1;
}

int
main(void)
{
	vfnarr_t arr = {add1, add1};
	vfnarr_t * const volatile p = &arr;
	return _Generic(p, vfnarr_t *: 1, int (* const volatile (*)[2])(int, ...): 2, default: 3);
}
