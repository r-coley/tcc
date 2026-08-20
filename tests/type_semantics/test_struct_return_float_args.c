struct ff {
	float a;
	float b;
};

static struct ff
make_ff(float a, float b)
{
	struct ff p;

	p.a = a + 1.0f;
	p.b = b + 2.0f;
	return p;
}

static int
use_ff(struct ff p)
{
	if (p.a != 2.5f)
		return 1;
	if (p.b != 4.5f)
		return 2;
	return 42;
}

int main(void)
{
	return use_ff(make_ff(1.5f, 2.5f));
}
