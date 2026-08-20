typedef struct MixedFloatDoubleFloat {
	float a;
	double b;
	float c;
} MixedFloatDoubleFloat;

MixedFloatDoubleFloat
make_mixed(float a, double b, float c)
{
	MixedFloatDoubleFloat v;
	v.a = a;
	v.b = b;
	v.c = c;
	return v;
}

int
consume_mixed(MixedFloatDoubleFloat v)
{
	if (v.a != 6.0f)
		return 1;
	if (v.b != 19.0)
		return 2;
	if (v.c != 8.0f)
		return 3;
	return 42;
}
