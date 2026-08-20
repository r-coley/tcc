typedef int (*vfn_t)(int, ...);
typedef vfn_t *vfnpp_t;
typedef vfn_t vfnarr_t[2];

static int
add1(int x, ...)
{
	return x + 1;
}

int
main(void)
{
	vfn_t p = add1;
	vfn_t const volatile fp = add1;
	vfnpp_t const volatile pp = &p;
	vfnarr_t arr = {add1, add1};
	vfnarr_t * const volatile ap = &arr;
	int total = 0;

	total += _Generic(fp, vfn_t: 10, default: 100);
	total += _Generic(fp, int (*)(int, ...): 20, default: 100);
	total += _Generic(pp, vfnpp_t: 30, default: 100);
	total += _Generic(pp, int (**)(int, ...): 40, default: 100);
	total += _Generic(ap, vfnarr_t *: 50, default: 100);
	total += _Generic(ap, int (*(*)[2])(int, ...): 60, default: 100);

	if (arr[1](41, 99) != 42)
		return 1;

	return total == 210 ? 42 : total;
}
