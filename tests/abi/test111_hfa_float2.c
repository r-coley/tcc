typedef struct HFAFloat2 {
	float a;
	float b;
} HFAFloat2;

extern HFAFloat2 make_pair(float a, float b);
extern int consume_pair(HFAFloat2 v);

int
main(void)
{
	HFAFloat2 v = make_pair(19.0f, 23.0f);
	return consume_pair(v);
}
