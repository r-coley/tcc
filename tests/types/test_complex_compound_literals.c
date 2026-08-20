#if defined(__STDC_VERSION__) && __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

int
main(void)
{
	_Complex double z = (_Complex double){ 3.0 };
	_Complex float w = (_Complex float){ 4.0f };
	_Complex double *pz = &(_Complex double){ 7.0 };
	double *zd = (double *)&z;
	float *wf = (float *)&w;
	double *pzd = (double *)pz;

	if (zd[0] != 3.0)
		return 1;
	if (zd[1] != 0.0)
		return 2;
	if (wf[0] != 4.0f)
		return 3;
	if (wf[1] != 0.0f)
		return 4;
	if (pzd[0] != 7.0)
		return 5;
	if (pzd[1] != 0.0)
		return 6;

	return 42;
}
