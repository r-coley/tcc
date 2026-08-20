#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#error _Generic requires C11 or later
#endif

typedef _Imaginary double imag_double_t;
typedef _Imaginary float imag_float_t;

int
main(void)
{
	_Imaginary double zd = (_Imaginary double)1.0;
	_Imaginary float zf = (_Imaginary float)2.0f;
	double d = 3.0;

	if (_Generic(zd, _Imaginary double: 10, double: 1, default: 2) != 10)
		return 1;
	if (_Generic(zf, _Imaginary float: 11, float: 1, default: 2) != 11)
		return 2;
	if (_Generic((imag_double_t)zd, imag_double_t: 12, default: 2) != 12)
		return 3;
	if (_Generic((imag_float_t)zf, imag_float_t: 13, default: 2) != 13)
		return 4;
	if (_Generic((double)zd, _Imaginary double: 1, double: 14, default: 2) != 14)
		return 5;
	if (_Generic(d, _Imaginary double: 1, double: 15, default: 2) != 15)
		return 6;
	return 42;
}
