typedef struct HFADouble3 {
	double a;
	double b;
	double c;
} HFADouble3;

extern HFADouble3 make_triple(double a, double b, double c);
extern int consume_triple(HFADouble3 v);

int
main(void)
{
	HFADouble3 v = make_triple(25.0, 26.0, 27.0);
	return consume_triple(v);
}
