int main(void)
{
#define TYPEOF(x) _Generic((x), int: 1, default: 99)
	float zf = 0.0f;
	float nzf = -3.0f;
	double zd = -0.0;
	double nzd = 2.0;
	int side = 0;

	if (TYPEOF(!zf) != 1 || TYPEOF(zf && nzd) != 1 || TYPEOF(zf || nzd) != 1)
		return 1;
	if (!zf != 1)
		return 2;
	if (!zd != 1)
		return 3;
	if (!nzf != 0)
		return 4;
	if ((nzd && nzf) != 1)
		return 5;
	if ((zf && ++side) != 0 || side != 0)
		return 6;
	if ((nzf || ++side) != 1 || side != 0)
		return 7;
	if ((zf || ++side) != 1 || side != 1)
		return 8;

	return 42;
}
