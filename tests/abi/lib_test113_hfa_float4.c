typedef struct HFAFloat4 {
	float a;
	float b;
	float c;
	float d;
} HFAFloat4;

HFAFloat4
make_quad(float a, float b, float c, float d)
{
	HFAFloat4 v;
	v.a = a;
	v.b = b;
	v.c = c;
	v.d = d;
	return v;
}

int
consume_quad(HFAFloat4 v)
{
	if (v.a != 10.0f)
		return 1;
	if (v.b != 11.0f)
		return 2;
	if (v.c != 12.0f)
		return 3;
	if (v.d != 13.0f)
		return 4;
	return 42;
}
