#define TYPEOF(x) _Generic((x), \
	int (*)(int): 1, \
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

int
main(void)
{
	int (*p_old)() = oldstyle;
	int (*p_proto)(int) = proto;

	if (TYPEOF(1 ? p_old : p_proto) != 1)
		return 1;
	if ((1 ? p_old : p_proto)(40) != 41)
		return 2;

	if (TYPEOF(0 ? p_old : p_proto) != 1)
		return 3;
	if ((0 ? p_old : p_proto)(40) != 42)
		return 4;

	if (TYPEOF(1 ? p_old : 0) != 1)
		return 5;
	if ((1 ? p_old : 0)(41) != 42)
		return 6;

	if (TYPEOF(0 ? 0 : p_proto) != 1)
		return 7;
	if ((0 ? 0 : p_proto)(40) != 42)
		return 8;

	return 42;
}
