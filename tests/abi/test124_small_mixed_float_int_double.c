typedef struct SmallMixedFloatIntDouble {
	float a;
	int tag;
	double b;
} SmallMixedFloatIntDouble;

extern SmallMixedFloatIntDouble make_mixed(float a, int tag, double b);
extern int consume_mixed(SmallMixedFloatIntDouble v);

int
main(void)
{
	SmallMixedFloatIntDouble v = make_mixed(9.0f, 17, 33.0);
	return consume_mixed(v);
}
