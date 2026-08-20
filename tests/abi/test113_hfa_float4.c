typedef struct HFAFloat4 {
	float a;
	float b;
	float c;
	float d;
} HFAFloat4;

extern HFAFloat4 make_quad(float a, float b, float c, float d);
extern int consume_quad(HFAFloat4 v);

int
main(void)
{
	HFAFloat4 v = make_quad(10.0f, 11.0f, 12.0f, 13.0f);
	return consume_quad(v);
}
