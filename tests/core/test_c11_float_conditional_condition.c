int main(void)
{
#define TYPEOF(x) _Generic((x), int: 1, double: 2, default: 99)
	float fz = 0.0f;
	float fnz = 1.0f;
	double dz = -0.0;
	double dnz = 2.0;
	int side = 0;
	int x = fz ? 1 : 2;
	int y = dnz ? 4 : 8;
	double d = dz ? 1.0 : 2.5;

	if (TYPEOF(fz ? 1 : 2) != 1)
		return 1;
	if (TYPEOF(dnz ? 1.0 : 2.0) != 2)
		return 2;
	if (x != 2 || y != 4 || d != 2.5)
		return 3;
	if ((fz ? ++side : 3) != 3 || side != 0)
		return 4;
	if ((fnz ? ++side : 3) != 1 || side != 1)
		return 5;

	return 42;
}
