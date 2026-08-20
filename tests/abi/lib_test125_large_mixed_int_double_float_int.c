typedef struct LargeMixedIntDoubleFloatInt {
	int tag;
	double b;
	float c;
	int code;
} LargeMixedIntDoubleFloatInt;

LargeMixedIntDoubleFloatInt
make_mixed(int tag, double b, float c, int code)
{
	LargeMixedIntDoubleFloatInt v;
	v.tag = tag;
	v.b = b;
	v.c = c;
	v.code = code;
	return v;
}

int
consume_mixed(LargeMixedIntDoubleFloatInt v)
{
	if (v.tag != 21)
		return 1;
	if (v.b != 37.0)
		return 2;
	if (v.c != 11.0f)
		return 3;
	if (v.code != 4)
		return 4;
	return 42;
}
