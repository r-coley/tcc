#if defined(__STDC_VERSION__) && __STDC_VERSION__ < 199901L
#error _Imaginary requires C99 or later
#endif

typedef _Imaginary double imaginary_double_t;
typedef float _Imaginary imaginary_float_reordered_t;

static imaginary_double_t global_double;
static imaginary_float_reordered_t global_float;

int
main(void)
{
	_Imaginary float local_float;
	double _Imaginary local_double_reordered;
	imaginary_double_t *double_ptr = &global_double;
	imaginary_float_reordered_t *float_ptr = &global_float;

	if (sizeof(_Imaginary float) != sizeof(float))
		return 1;
	if (sizeof(_Imaginary double) != sizeof(double))
		return 2;
	if (sizeof(imaginary_double_t) != sizeof(double))
		return 3;
	if (sizeof(imaginary_float_reordered_t) != sizeof(float))
		return 4;
	if (sizeof(global_double) != sizeof(double))
		return 5;
	if (sizeof(global_float) != sizeof(float))
		return 6;
	if (sizeof(local_float) != sizeof(float))
		return 7;
	if (sizeof(local_double_reordered) != sizeof(double))
		return 8;
	if (sizeof(*double_ptr) != sizeof(double))
		return 9;
	if (sizeof(*float_ptr) != sizeof(float))
		return 10;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
	if (_Alignof(_Imaginary float) != _Alignof(float))
		return 11;
	if (_Alignof(_Imaginary double) != _Alignof(double))
		return 12;
	if (_Alignof(double _Imaginary) != _Alignof(double))
		return 13;
#endif
	return 42;
}
