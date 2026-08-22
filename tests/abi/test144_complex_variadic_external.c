#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

extern int consume_complex_variadic_external(int tag, ...);

int
main(void)
{
	_Complex float first;
	_Complex double second;
	float *first_parts = (float *)&first;
	double *second_parts = (double *)&second;

	first_parts[0] = 1.5f;
	first_parts[1] = 2.5f;
	second_parts[0] = 3.5;
	second_parts[1] = 4.5;
	return consume_complex_variadic_external(7, first, second);
}
