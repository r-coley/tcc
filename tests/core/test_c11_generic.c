typedef int int_array[4];

static int pick_int(int v) { return v + 1; }
static int pick_long(long v) { return (int)v + 2; }

int main(void)
{
	int i = 10;
	const int ci = 20;
	long l = 30;
	int_array a;

	if (_Generic(i, int: 1, long: 2, default: 3) != 1)
		return 1;
	if (_Generic(l, int: 1, long: 2, default: 3) != 2)
		return 2;
	if (_Generic(ci, int: 1, long: 2, default: 3) != 1)
		return 3;
	if (_Generic(a, int_array: 1, int *: 2, default: 3) != 2)
		return 4;
	if (_Generic("x", char *: 1, const char *: 2, default: 3) != 1)
		return 5;
	if (_Generic(i, int: pick_int, long: pick_long)(i) != 11)
		return 6;

	return 42;
}
