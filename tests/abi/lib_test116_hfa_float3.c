typedef struct HFAFloat3 {
	float a;
	float b;
	float c;
} HFAFloat3;

HFAFloat3
make_triple(float a, float b, float c)
{
	HFAFloat3 v;
	v.a = a;
	v.b = b;
	v.c = c;
	return v;
}

int
consume_triple(HFAFloat3 v)
{
	if (v.a != 20.0f)
		return 1;
	if (v.b != 21.0f)
		return 2;
	if (v.c != 22.0f)
		return 3;
	return 42;
}
