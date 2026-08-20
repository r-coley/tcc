#define TYPEOF(x) _Generic((x), \
	float: 1, \
	double: 2, \
	int: 3, \
	default: 99)

int
main(void)
{
	float f = 1.25f;
	const float cf = 1.0f;
	double d = 2.5;
	volatile double vd = 2.0;
	int i = 3;

	if (TYPEOF(f) != 1)
		return 1;
	if (TYPEOF(d) != 2)
		return 2;
	if (TYPEOF(f + f) != 1)
		return 3;
	if (TYPEOF(f + d) != 2)
		return 4;
	if (TYPEOF(d + i) != 2)
		return 5;
	if (TYPEOF(f + i) != 1)
		return 6;
	if (TYPEOF(cf) != 1)
		return 7;
	if (TYPEOF(vd) != 2)
		return 8;
	if (TYPEOF(+cf) != 1)
		return 9;
	if (TYPEOF(+vd) != 2)
		return 10;

	return 42;
}
