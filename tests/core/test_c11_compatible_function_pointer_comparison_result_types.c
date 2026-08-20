#define TYPEOF(x) _Generic((x), \
	int: 1, \
	default: 99)

static int
oldstyle(x)
int x;
{
	return x + 1;
}

static int
proto(int x)
{
	return x + 2;
}

static int
var_a(int x, ...)
{
	return x + 3;
}

int
main(void)
{
	int (*fp_old)() = oldstyle;
	int (*fp_proto)(int) = proto;
	int (*vf_a)(int, ...) = var_a;
	int (*vf_b)(int, ...) = var_a;

	if (TYPEOF(fp_old == fp_proto) != 1)
		return 1;
	if (TYPEOF(fp_old != fp_proto) != 1)
		return 2;
	if (TYPEOF(fp_old != 0) != 1)
		return 3;
	if (TYPEOF(0 != fp_proto) != 1)
		return 4;
	if (TYPEOF(0 == fp_old) != 1)
		return 5;
	if (TYPEOF(vf_a == vf_b) != 1)
		return 6;
	if (TYPEOF(vf_a != 0) != 1)
		return 7;
	if (TYPEOF(0 != vf_b) != 1)
		return 8;

	if ((fp_old == fp_old) != 1)
		return 9;
	if ((fp_old == fp_proto) != 0)
		return 10;
	if ((fp_old != 0) != 1)
		return 11;
	if ((0 != fp_proto) != 1)
		return 12;
	if ((0 == fp_old) != 0)
		return 13;
	if ((vf_a == vf_b) != 1)
		return 14;
	if ((vf_a != 0) != 1)
		return 15;
	if ((0 != vf_b) != 1)
		return 16;

	return 42;
}
