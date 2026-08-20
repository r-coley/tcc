#if defined(__STDC_VERSION__) && __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

int
main(void)
{
	_Complex double z;
	void *vp = &z;
	_Complex double *pz = (_Complex double *)vp;
	double *parts = (double *)pz;

	if (sizeof(*pz) != 16)
		return 1;

	parts[0] = 5.0;
	parts[1] = 7.0;

	if (((double *)pz)[0] != 5.0)
		return 2;
	if (((double *)pz)[1] != 7.0)
		return 3;

	return 42;
}
