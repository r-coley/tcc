typedef struct LargeMixedIntDoubleFloatIntArg {
	int tag;
	double b;
	float c;
	int code;
} LargeMixedIntDoubleFloatIntArg;

int
consume_mixed_arg(LargeMixedIntDoubleFloatIntArg v)
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
