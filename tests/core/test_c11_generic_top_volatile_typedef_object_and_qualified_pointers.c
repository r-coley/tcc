typedef int *ip;
typedef const int *cip;
typedef void *vp;
typedef const long *clp;
typedef const void *cvp_t;
typedef volatile void *vvp_t;
typedef const volatile void *cvvp_t;

int
main(void)
{
	int value = 0;
	long lvalue = 0;
	ip volatile p = &value;
	ip const volatile cp = &value;
	cip volatile qcp = &value;
	cip const volatile cvqcp = &value;
	vp volatile pv = &value;
	vp const volatile cvpv = &value;
	clp volatile lp = &lvalue;
	clp const volatile cvlp = &lvalue;
	cvp_t volatile cvp = &value;
	cvp_t const volatile cvcvp = &value;
	vvp_t volatile vvp = &value;
	vvp_t const volatile cvvvp = &value;
	cvvp_t volatile cvvp = &value;
	cvvp_t const volatile cvcvvp = &value;
	int total = 0;

	total += _Generic(p, ip: 10, default: 100);
	total += _Generic(p, int *: 20, default: 100);
	total += _Generic(cp, ip: 30, default: 100);
	total += _Generic(cp, int *: 40, default: 100);
	total += _Generic(qcp, cip: 50, default: 100);
	total += _Generic(qcp, const int *: 60, default: 100);
	total += _Generic(cvqcp, cip: 70, default: 100);
	total += _Generic(cvqcp, const int *: 80, default: 100);
	total += _Generic(pv, vp: 90, default: 100);
	total += _Generic(pv, void *: 100, default: 1000);
	total += _Generic(cvpv, vp: 110, default: 100);
	total += _Generic(cvpv, void *: 120, default: 1000);
	total += _Generic(lp, clp: 130, default: 100);
	total += _Generic(lp, const long *: 140, default: 1000);
	total += _Generic(cvlp, clp: 150, default: 100);
	total += _Generic(cvlp, const long *: 160, default: 1000);
	total += _Generic(cvp, cvp_t: 170, default: 100);
	total += _Generic(cvp, const void *: 180, default: 1000);
	total += _Generic(cvcvp, cvp_t: 190, default: 100);
	total += _Generic(cvcvp, const void *: 200, default: 1000);
	total += _Generic(vvp, vvp_t: 210, default: 100);
	total += _Generic(vvp, volatile void *: 220, default: 1000);
	total += _Generic(cvvvp, vvp_t: 230, default: 100);
	total += _Generic(cvvvp, volatile void *: 240, default: 1000);
	total += _Generic(cvvp, cvvp_t: 250, default: 100);
	total += _Generic(cvvp, const volatile void *: 260, default: 1000);
	total += _Generic(cvcvvp, cvvp_t: 270, default: 100);
	total += _Generic(cvcvvp, const volatile void *: 280, default: 1000);

	return total == 4060 ? 42 : total;
}
