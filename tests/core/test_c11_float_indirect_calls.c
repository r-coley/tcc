float addf(float a, float b)
{
	return a + b;
}

float subf(float a, float b)
{
	return a - b;
}

double addd(double a, double b)
{
	return a + b;
}

double mixd(int tag, float f, double d)
{
	return tag ? f : d;
}

int main(void)
{
	float (*pf)(float, float) = addf;
	double (*pd)(double, double) = addd;
	double (*pmix)(int, float, double) = mixd;
	float v;
	double d;

	v = pf(19.5f, 22.5f);
	if (v != 42.0f)
		return 1;

	pf = subf;
	v = (*pf)(50.0f, 8.0f);
	if (v != 42.0f)
		return 2;

	d = pd(19.5, 22.5);
	if (d != 42.0)
		return 3;

	d = pmix(1, 42.0f, 1.0);
	if (d != 42.0)
		return 4;

	d = pmix(0, 1.0f, 42.0);
	if (d != 42.0)
		return 5;

	return 42;
}
