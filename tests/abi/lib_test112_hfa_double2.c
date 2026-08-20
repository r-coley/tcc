typedef struct HFADouble2 {
	double a;
	double b;
} HFADouble2;

HFADouble2
make_pair(double a, double b)
{
	HFADouble2 v;
	v.a = a;
	v.b = b;
	return v;
}

int
consume_pair(HFADouble2 v)
{
	if (v.a != 18.0)
		return 1;
	if (v.b != 24.0)
		return 2;
	return 42;
}
