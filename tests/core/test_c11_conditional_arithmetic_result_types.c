#define TYPEOF(x) _Generic((x), \
	float: 1, \
	double: 2, \
	int: 3, \
	unsigned int: 4, \
	long: 5, \
	unsigned long: 6, \
	default: 99)

int
main(void)
{
	float f = 1.0f;
	const float cf = 1.5f;
	double d = 2.0;
	volatile double vd = 2.5;
	int i = 1;
	int z = 0;
	unsigned int u = 2U;

	if (TYPEOF(i ? f : d) != 2)
		return 1;
	if (TYPEOF(i ? f : i) != 1)
		return 2;
	if (TYPEOF(i ? d : u) != 2)
		return 3;
	if (TYPEOF(i ? i : u) != 4)
		return 4;
	if (TYPEOF(i ? cf : 2) != 1)
		return 5;
	if (TYPEOF(i ? cf : vd) != 2)
		return 6;
	if (TYPEOF(z ? vd : cf) != 2)
		return 7;
	if ((i ? cf : 2) != 1.5f)
		return 8;
	if ((z ? cf : 2.25f) != 2.25f)
		return 9;
	if ((i ? vd : cf) != 2.5)
		return 10;

	return 42;
}
