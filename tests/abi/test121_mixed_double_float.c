typedef struct MixedDoubleFloat {
	double a;
	float b;
} MixedDoubleFloat;

extern MixedDoubleFloat make_mixed(double a, float b);
extern int consume_mixed(MixedDoubleFloat v);

int
main(void)
{
	MixedDoubleFloat v = make_mixed(31.0, 12.0f);
	return consume_mixed(v);
}
