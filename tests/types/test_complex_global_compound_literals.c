#if defined(__STDC_VERSION__) && __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

static _Complex double global_value = 5.0;
static _Complex float global_value_f = { 2.0f };
static _Complex double *global_ptr = &(_Complex double){ 7.0 };

int
main(void)
{
	double *gv = (double *)&global_value;
	float *gf = (float *)&global_value_f;
	double *gp = (double *)global_ptr;

	if (gv[0] != 5.0)
		return 1;
	if (gv[1] != 0.0)
		return 2;
	if (gf[0] != 2.0f)
		return 3;
	if (gf[1] != 0.0f)
		return 4;
	if (gp[0] != 7.0)
		return 5;
	if (gp[1] != 0.0)
		return 6;

	return 42;
}
