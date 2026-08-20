typedef int **ipp;
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
	int x = 0;
	int *p = &x;
	ipp pp = &p;
	ipp const cpp = &p;
	ipp volatile vpp = &p;
	ipp const volatile cvpp = &p;
	fn_t fp = f;
	fnpp_t fpp = &fp;
	fnpp_t const cfpp = &fp;
	fnpp_t volatile vfpp = &fp;
	fnpp_t const volatile cvfpp = &fp;
	int total = 0;

	total += _Generic(pp, ipp: 10, default: 100);
	total += _Generic(pp, int **: 20, default: 100);
	total += _Generic(cpp, ipp: 30, default: 100);
	total += _Generic(cpp, int **: 40, default: 100);
	total += _Generic(vpp, ipp: 50, default: 100);
	total += _Generic(vpp, int **: 60, default: 100);
	total += _Generic(cvpp, ipp: 70, default: 100);
	total += _Generic(cvpp, int **: 80, default: 100);
	total += _Generic(fpp, fnpp_t: 90, default: 1000);
	total += _Generic(fpp, int (**)(int): 100, default: 1000);
	total += _Generic(cfpp, fnpp_t: 110, default: 1000);
	total += _Generic(cfpp, int (**)(int): 120, default: 1000);
	total += _Generic(vfpp, fnpp_t: 130, default: 1000);
	total += _Generic(vfpp, int (**)(int): 140, default: 1000);
	total += _Generic(cvfpp, fnpp_t: 150, default: 1000);
	total += _Generic(cvfpp, int (**)(int): 160, default: 1000);

	return total == 1360 ? 42 : total;
}
