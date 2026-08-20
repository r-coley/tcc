typedef struct HFADouble2 {
	double a;
	double b;
} HFADouble2;

extern HFADouble2 make_pair(double a, double b);
extern int consume_pair(HFADouble2 v);

int
main(void)
{
	HFADouble2 v = make_pair(18.0, 24.0);
	return consume_pair(v);
}
