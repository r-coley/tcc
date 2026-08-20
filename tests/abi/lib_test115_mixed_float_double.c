typedef struct MixedFloatDouble {
	float a;
	double b;
} MixedFloatDouble;

MixedFloatDouble
make_mixed(float a, double b)
{
	MixedFloatDouble v;
	v.a = a;
	v.b = b;
	return v;
}

int
consume_mixed(MixedFloatDouble v)
{
	if (v.a != 18.0f)
		return 1;
	if (v.b != 24.0)
		return 2;
	return 42;
}
