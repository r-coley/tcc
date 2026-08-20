#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#error _Generic requires C11 or later
#endif

typedef _Complex double complex_double_t;
typedef _Complex float complex_float_t;

int
main(void)
{
	_Complex double zd = (_Complex double)1.0;
	_Complex float zf = (_Complex float)2.0f;
	double d = 3.0;

	if (_Generic(zd, _Complex double: 10, double: 1, default: 2) != 10)
		return 1;
	if (_Generic(zf, _Complex float: 11, float: 1, default: 2) != 11)
		return 2;
	if (_Generic((complex_double_t)zd, complex_double_t: 12, default: 2) != 12)
		return 3;
	if (_Generic((complex_float_t)zf, complex_float_t: 13, default: 2) != 13)
		return 4;
	if (_Generic((double)zd, _Complex double: 1, double: 14, default: 2) != 14)
		return 5;
	if (_Generic(d, _Complex double: 1, double: 15, default: 2) != 15)
		return 6;
	return 42;
}
