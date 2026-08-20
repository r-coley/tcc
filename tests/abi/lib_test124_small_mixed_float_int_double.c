typedef struct SmallMixedFloatIntDouble {
	float a;
	int tag;
	double b;
} SmallMixedFloatIntDouble;

SmallMixedFloatIntDouble
make_mixed(float a, int tag, double b)
{
	SmallMixedFloatIntDouble v;
	v.a = a;
	v.tag = tag;
	v.b = b;
	return v;
}

int
consume_mixed(SmallMixedFloatIntDouble v)
{
	if (v.a != 9.0f)
		return 1;
	if (v.tag != 17)
		return 2;
	if (v.b != 33.0)
		return 3;
	return 42;
}
