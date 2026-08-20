typedef struct LargeMixedIntDoubleFloatIntArg {
	int tag;
	double b;
	float c;
	int code;
} LargeMixedIntDoubleFloatIntArg;

extern int consume_mixed_arg(LargeMixedIntDoubleFloatIntArg v);

int
main(void)
{
	LargeMixedIntDoubleFloatIntArg v;

	v.tag = 21;
	v.b = 37.0;
	v.c = 11.0f;
	v.code = 4;
	return consume_mixed_arg(v);
}
