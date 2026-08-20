typedef struct MixedFloatDouble {
	float a;
	double b;
} MixedFloatDouble;

extern MixedFloatDouble make_mixed(float a, double b);
extern int consume_mixed(MixedFloatDouble v);

int
main(void)
{
	MixedFloatDouble v = make_mixed(18.0f, 24.0);
	return consume_mixed(v);
}
