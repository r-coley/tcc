typedef struct MixedDoubleFloat {
	double a;
	float b;
} MixedDoubleFloat;

MixedDoubleFloat
make_mixed(double a, float b)
{
	MixedDoubleFloat v;
	v.a = a;
	v.b = b;
	return v;
}

int
consume_mixed(MixedDoubleFloat v)
{
	if (v.a != 31.0)
		return 1;
	if (v.b != 12.0f)
		return 2;
	return 42;
}
