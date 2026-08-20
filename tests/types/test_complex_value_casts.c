#if defined(__STDC_VERSION__) && __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

int
main(void)
{
	_Complex double z = (_Complex double)5.0;
	_Complex float w = (_Complex float)3;
	_Complex float wf2;
	_Complex double zd2;
	double zr = (double)z;
	int wi = (int)w;
	double *zd = (double *)&z;
	float *wf = (float *)&w;
	float *wf2p = (float *)&wf2;
	double *zd2p = (double *)&zd2;

	if (sizeof(z) != 16)
		return 1;
	if (sizeof(w) != 8)
		return 2;

	if (zd[0] != 5.0)
		return 3;
	if (zd[1] != 0.0)
		return 4;

	if (wf[0] != 3.0f)
		return 5;
	if (wf[1] != 0.0f)
		return 6;
	if (zr != 5.0)
		return 7;
	if (wi != 3)
		return 8;

	zd[0] = 1.5;
	zd[1] = 2.5;
	wf2 = (_Complex float)z;
	if (wf2p[0] != 1.5f)
		return 9;
	if (wf2p[1] != 2.5f)
		return 10;

	wf[0] = 4.0f;
	wf[1] = 6.0f;
	zd2 = (_Complex double)w;
	if (zd2p[0] != 4.0)
		return 11;
	if (zd2p[1] != 6.0)
		return 12;

	return 42;
}
