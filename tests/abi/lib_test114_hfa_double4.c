typedef struct HFADouble4 {
	double a;
	double b;
	double c;
	double d;
} HFADouble4;

HFADouble4
make_quad(double a, double b, double c, double d)
{
	HFADouble4 v;
	v.a = a;
	v.b = b;
	v.c = c;
	v.d = d;
	return v;
}

int
consume_quad(HFADouble4 v)
{
	if (v.a != 14.0)
		return 1;
	if (v.b != 15.0)
		return 2;
	if (v.c != 16.0)
		return 3;
	if (v.d != 17.0)
		return 4;
	return 42;
}
