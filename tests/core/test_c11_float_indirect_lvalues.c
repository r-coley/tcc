int main(void)
{
	float a[2];
	double b[2];
	float f = 1.0f;
	double d = 2.0;
	float *pf = &f;
	double *pd = &d;

	a[0] = 1.5f;
	b[1] = 2.5;
	a[0] += 1.0f;
	b[1] *= 2.0;
	if (a[0] != 2.5f || b[1] != 5.0)
		return 1;

	*pf += 2.0f;
	*pd += *pf;
	if (f != 3.0f || d != 5.0)
		return 2;

	return 42;
}
