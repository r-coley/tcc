#if defined(__STDC_VERSION__) && __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

int
main(void)
{
	_Complex double a = (_Complex double)1.0;
	_Complex double b = (_Complex double)2.0;
	_Complex double c;
	_Complex double d;
	_Complex float fa = (_Complex float)3.0f;
	_Complex float fb = (_Complex float)1.0f;
	_Complex float fc;
	double *cp;
	float *fcp;

	((double *)&a)[1] = 4.0;
	((double *)&b)[1] = 5.0;
	c = a + b;
	cp = (double *)&c;
	if (cp[0] != 3.0)
		return 1;
	if (cp[1] != 9.0)
		return 2;

	c = b - a;
	cp = (double *)&c;
	if (cp[0] != 1.0)
		return 3;
	if (cp[1] != 1.0)
		return 4;

	c = -a;
	cp = (double *)&c;
	if (cp[0] != -1.0)
		return 5;
	if (cp[1] != -4.0)
		return 6;

	a += b;
	cp = (double *)&a;
	if (cp[0] != 3.0)
		return 7;
	if (cp[1] != 9.0)
		return 8;

	a -= b;
	cp = (double *)&a;
	if (cp[0] != 1.0)
		return 9;
	if (cp[1] != 4.0)
		return 10;

	a = (_Complex double)1.0;
	b = (_Complex double)2.0;
	((double *)&a)[1] = 4.0;
	((double *)&b)[1] = 5.0;
	d = a * b;
	cp = (double *)&d;
	if (cp[0] != -18.0)
		return 11;
	if (cp[1] != 13.0)
		return 12;

	a *= b;
	cp = (double *)&a;
	if (cp[0] != -18.0)
		return 13;
	if (cp[1] != 13.0)
		return 14;

	a = (_Complex double)4.0;
	b = (_Complex double)1.0;
	((double *)&a)[1] = 2.0;
	((double *)&b)[1] = -1.0;
	d = a / b;
	cp = (double *)&d;
	if (cp[0] != 1.0)
		return 15;
	if (cp[1] != 3.0)
		return 16;

	a /= b;
	cp = (double *)&a;
	if (cp[0] != 1.0)
		return 17;
	if (cp[1] != 3.0)
		return 18;

	((float *)&fa)[1] = 2.0f;
	((float *)&fb)[1] = 6.0f;
	fc = fa + fb;
	fcp = (float *)&fc;
	if (fcp[0] != 4.0f)
		return 19;
	if (fcp[1] != 8.0f)
		return 20;

	return 42;
}
