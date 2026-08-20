#if defined(__STDC_VERSION__) && __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

union ComplexLongDoubleParts {
	_Complex long double value;
	long double parts[2];
};

int
main(void)
{
	_Complex long double a = (_Complex long double)1.25L;
	_Complex long double b = (_Complex long double)3.0L;
	union ComplexLongDoubleParts ua;
	union ComplexLongDoubleParts ub;
	volatile int cond = 0;

	ua.value = a;
	ub.value = b;
	ua.parts[1] = 2.5L;
	ub.parts[1] = 4.0L;

	ua.value = cond ? ua.value : ub.value;
	if (ua.parts[0] != 3.0L)
		return 1;
	if (ua.parts[1] != 4.0L)
		return 2;

	cond = 1;
	ua.value = cond ? ua.value : ub.value;
	if (ua.parts[0] != 3.0L)
		return 3;
	if (ua.parts[1] != 4.0L)
		return 4;

	return 42;
}
