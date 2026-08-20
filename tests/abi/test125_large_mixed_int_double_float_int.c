typedef struct LargeMixedIntDoubleFloatInt {
	int tag;
	double b;
	float c;
	int code;
} LargeMixedIntDoubleFloatInt;

extern LargeMixedIntDoubleFloatInt make_mixed(int tag, double b, float c, int code);
extern int consume_mixed(LargeMixedIntDoubleFloatInt v);

int
main(void)
{
	LargeMixedIntDoubleFloatInt v = make_mixed(21, 37.0, 11.0f, 4);
	return consume_mixed(v);
}
