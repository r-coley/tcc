struct FPair {
	float a;
	float b;
};

struct DPair {
	double a;
	double b;
};

static struct FPair make_fpair(float a, float b)
{
	struct FPair p = { a, b };
	return p;
}

static struct DPair make_dpair(double a, double b)
{
	struct DPair p = { a, b };
	return p;
}

static int check_fpair(struct FPair p)
{
	return p.a == 1.5f && p.b == 2.5f;
}

static int check_dpair(struct DPair p)
{
	return p.a == 1.25 && p.b == 2.75;
}

int main(void)
{
	struct FPair fp = make_fpair(1.5f, 2.5f);
	struct DPair dp = make_dpair(1.25, 2.75);

	if (!check_fpair(fp))
		return 1;
	if (!check_dpair(dp))
		return 2;
	return 42;
}
