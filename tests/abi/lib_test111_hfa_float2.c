typedef struct HFAFloat2 {
	float a;
	float b;
} HFAFloat2;

HFAFloat2
make_pair(float a, float b)
{
	HFAFloat2 v;
	v.a = a;
	v.b = b;
	return v;
}

int
consume_pair(HFAFloat2 v)
{
	if (v.a != 19.0f)
		return 1;
	if (v.b != 23.0f)
		return 2;
	return 42;
}
