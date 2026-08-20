typedef int *ip;
typedef const int *cip;
typedef void *vp;

int
main(void)
{
	int value = 0;
	ip const p = &value;
	cip const cp = &value;
	vp const pv = &value;
	int total = 0;

	total += _Generic(p, ip: 10, default: 100);
	total += _Generic(p, int *: 20, default: 100);
	total += _Generic(cp, cip: 30, default: 100);
	total += _Generic(cp, const int *: 40, default: 100);
	total += _Generic(pv, vp: 50, default: 100);
	total += _Generic(pv, void *: 60, default: 100);

	return total == 210 ? 42 : total;
}
