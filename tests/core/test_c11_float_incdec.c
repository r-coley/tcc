int main(void)
{
#define TYPEOF(x) _Generic((x), float: 1, double: 2, default: 99)
	float f = 1.0f;
	double d = 4.0;
	float oldf;
	double oldd;

	if (TYPEOF(++f) != 1)
		return 1;
	if (++f != 2.0f)
		return 2;
	oldf = f++;
	if (oldf != 2.0f || f != 3.0f)
		return 3;

	if (TYPEOF(--d) != 2)
		return 4;
	if (--d != 3.0)
		return 5;
	oldd = d--;
	if (oldd != 3.0 || d != 2.0)
		return 6;

	return 42;
}
