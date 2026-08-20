typedef const long *clp;
typedef const void *cvp_t;
typedef volatile void *vvp_t;
typedef const volatile void *cvvp_t;

int
main(void)
{
	long value = 0;
	clp const cp = &value;
	cvp_t const cvp = &value;
	vvp_t const vvp = &value;
	cvvp_t const cvvp = &value;
	int total = 0;

	total += _Generic(cp, clp: 10, default: 100);
	total += _Generic(cp, const long *: 20, default: 100);
	total += _Generic(cvp, cvp_t: 30, default: 100);
	total += _Generic(cvp, const void *: 40, default: 100);
	total += _Generic(vvp, vvp_t: 50, default: 100);
	total += _Generic(vvp, volatile void *: 60, default: 100);
	total += _Generic(cvvp, cvvp_t: 70, default: 100);
	total += _Generic(cvvp, const volatile void *: 80, default: 100);

	return total == 360 ? 42 : total;
}
