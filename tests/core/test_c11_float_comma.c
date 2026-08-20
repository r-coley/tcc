int main(void)
{
#define TYPEOF(x) _Generic((x), int: 1, double: 2, default: 99)
	float f = 1.0f;
	double d = 2.0;
	int x;

	x = (f += 1.0f, d += 2.0, (int)(f + d));
	if (x != 6)
		return 1;
	if (TYPEOF((f += 1.0f, x)) != 1)
		return 2;
	if (TYPEOF((x, d + f)) != 2)
		return 3;
	if ((f += 1.0f, d += 1.0, (f == 3.0f && d == 5.0)) != 1)
		return 4;

	return 42;
}
