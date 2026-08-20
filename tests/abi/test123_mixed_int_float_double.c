typedef struct MixedIntFloatDouble {
	int tag;
	float a;
	double b;
} MixedIntFloatDouble;

extern MixedIntFloatDouble make_mixed(int tag, float a, double b);
extern int consume_mixed(MixedIntFloatDouble v);

int
main(void)
{
	MixedIntFloatDouble v = make_mixed(7, 5.0f, 29.0);
	return consume_mixed(v);
}
