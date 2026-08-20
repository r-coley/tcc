typedef struct HFADouble4 {
	double a;
	double b;
	double c;
	double d;
} HFADouble4;

extern HFADouble4 make_quad(double a, double b, double c, double d);
extern int consume_quad(HFADouble4 v);

int
main(void)
{
	HFADouble4 v = make_quad(14.0, 15.0, 16.0, 17.0);
	return consume_quad(v);
}
