typedef struct MixedFloatDoubleFloat {
	float a;
	double b;
	float c;
} MixedFloatDoubleFloat;

extern MixedFloatDoubleFloat make_mixed(float a, double b, float c);
extern int consume_mixed(MixedFloatDoubleFloat v);

int
main(void)
{
	MixedFloatDoubleFloat v = make_mixed(6.0f, 19.0, 8.0f);
	return consume_mixed(v);
}
