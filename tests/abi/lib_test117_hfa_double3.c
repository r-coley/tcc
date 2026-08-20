typedef struct HFADouble3 {
	double a;
	double b;
	double c;
} HFADouble3;

HFADouble3
make_triple(double a, double b, double c)
{
	HFADouble3 v;
	v.a = a;
	v.b = b;
	v.c = c;
	return v;
}

int
consume_triple(HFADouble3 v)
{
	if (v.a != 25.0)
		return 1;
	if (v.b != 26.0)
		return 2;
	if (v.c != 27.0)
		return 3;
	return 42;
}
