float addf(float a, float b) { return a + b; }
float subf(float a, float b) { return a - b; }
double addd(double a, double b) { return a + b; }
double subd(double a, double b) { return a - b; }

float (*gpf)(float, float) = addf;
double (*gpd)(double, double) = addd;

float (*choosef(int add))(float, float)
{
	return add ? addf : subf;
}

double (*choosed(int add))(double, double)
{
	return add ? addd : subd;
}

int main(void)
{
	float (*pf)(float, float) = addf;
	float (*qf)(float, float) = subf;
	double (*pd)(double, double) = addd;
	double (*qd)(double, double) = subd;

	if (gpf(19.5f, 22.5f) != 42.0f)
		return 1;
	if (gpd(19.5, 22.5) != 42.0)
		return 2;

	if ((1 ? pf : qf)(19.5f, 22.5f) != 42.0f)
		return 3;
	if ((0 ? pf : qf)(50.0f, 8.0f) != 42.0f)
		return 4;
	if ((1 ? pd : qd)(19.5, 22.5) != 42.0)
		return 5;
	if ((0 ? pd : qd)(50.0, 8.0) != 42.0)
		return 6;

	if (choosef(1)(19.5f, 22.5f) != 42.0f)
		return 7;
	if (choosef(0)(50.0f, 8.0f) != 42.0f)
		return 8;
	if (choosed(1)(19.5, 22.5) != 42.0)
		return 9;
	if (choosed(0)(50.0, 8.0) != 42.0)
		return 10;

	return 42;
}
