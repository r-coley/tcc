typedef struct MixedIntFloatDouble {
	int tag;
	float a;
	double b;
} MixedIntFloatDouble;

MixedIntFloatDouble
make_mixed(int tag, float a, double b)
{
	MixedIntFloatDouble v;
	v.tag = tag;
	v.a = a;
	v.b = b;
	return v;
}

int
consume_mixed(MixedIntFloatDouble v)
{
	if (v.tag != 7)
		return 1;
	if (v.a != 5.0f)
		return 2;
	if (v.b != 29.0)
		return 3;
	return 42;
}
