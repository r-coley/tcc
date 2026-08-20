#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#error _Generic requires C11 or later
#endif

typedef _Imaginary long double imag_long_double_t;

int
main(void)
{
	_Imaginary long double zld = (_Imaginary long double)1.0L;
	long double d = 2.0L;

	if (_Generic(zld, _Imaginary long double: 10, long double: 1, default: 2) != 10)
		return 1;
	if (_Generic((imag_long_double_t)zld, imag_long_double_t: 11, default: 2) != 11)
		return 2;
	if (_Generic((long double)zld, _Imaginary long double: 1, long double: 12, default: 2) != 12)
		return 3;
	if (_Generic(d, _Imaginary long double: 1, long double: 13, default: 2) != 13)
		return 4;
	return 42;
}
