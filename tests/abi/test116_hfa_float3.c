typedef struct HFAFloat3 {
	float a;
	float b;
	float c;
} HFAFloat3;

extern HFAFloat3 make_triple(float a, float b, float c);
extern int consume_triple(HFAFloat3 v);

int
main(void)
{
	HFAFloat3 v = make_triple(20.0f, 21.0f, 22.0f);
	return consume_triple(v);
}
