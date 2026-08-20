#if defined(__STDC_VERSION__) && __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

typedef _Complex long double complex_long_double_t;
typedef long double _Complex complex_long_double_reordered_t;

static _Complex long double
complex_long_double_identity(_Complex long double x)
{
	return x;
}

static _Complex long double
complex_long_double_outer(_Complex long double x)
{
	return complex_long_double_identity(x);
}

union ComplexLongDoubleParts {
	_Complex long double value;
	long double parts[2];
};

struct ComplexLongDoubleBox {
	_Complex long double value;
};

int
main(void)
{
	_Complex long double a = (_Complex long double)1.25L;
	_Complex long double b = (_Complex long double)2.5L;
	long double _Complex reordered = (long double _Complex)7.0L;
	_Complex long double c;
	_Complex long double d;
	_Complex double zd = (_Complex double)4.0;
	complex_long_double_t alias_value;
	complex_long_double_reordered_t reordered_alias;
	struct ComplexLongDoubleBox box;
	union ComplexLongDoubleParts ua;
	union ComplexLongDoubleParts ub;
	union ComplexLongDoubleParts uc;
	union ComplexLongDoubleParts ud;
	union ComplexLongDoubleParts ualias;
	union ComplexLongDoubleParts ubox;
	union ComplexLongDoubleParts ureordered;
	long double real_part;

	if (sizeof(_Complex long double) != sizeof(long double) * 2)
		return 1;
	if (sizeof(complex_long_double_t) != sizeof(long double) * 2)
		return 2;
	if (sizeof(complex_long_double_reordered_t) != sizeof(long double _Complex))
		return 3;
	if (sizeof(struct ComplexLongDoubleBox) != sizeof(_Complex long double))
		return 14;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
	if (_Alignof(_Complex long double) != _Alignof(long double))
		return 4;
	if (_Alignof(long double _Complex) != _Alignof(long double))
		return 15;
#endif

	ua.value = a;
	ub.value = b;
	ua.parts[1] = 3.75L;
	ub.parts[1] = -0.5L;
	a = ua.value;
	b = ub.value;

	c = a + b;
	uc.value = c;
	if (uc.parts[0] != 3.75L)
		return 5;
	if (uc.parts[1] != 3.25L)
		return 6;

	d = a * b;
	ud.value = d;
	if (ud.parts[0] != 5.0L)
		return 7;
	if (ud.parts[1] != 8.75L)
		return 8;

	((double *)&zd)[1] = 6.0;
	alias_value = (_Complex long double)zd;
	ualias.value = alias_value;
	if (ualias.parts[0] != 4.0L)
		return 9;
	if (ualias.parts[1] != 6.0L)
		return 10;

	reordered_alias = reordered;
	ureordered.value = reordered_alias;
	if (ureordered.parts[0] != 7.0L)
		return 16;
	if (ureordered.parts[1] != 0.0L)
		return 17;

	box.value = alias_value;
	ubox.value = box.value;
	if (ubox.parts[0] != 4.0L)
		return 11;
	if (ubox.parts[1] != 6.0L)
		return 12;

	real_part = (long double)alias_value;
	if (real_part != 4.0L)
		return 13;

	uc.value = a + zd;
	if (uc.parts[0] != 5.25L)
		return 18;
	if (uc.parts[1] != 9.75L)
		return 19;

	ud.value = zd;
	if (ud.parts[0] != 4.0L)
		return 20;
	if (ud.parts[1] != 6.0L)
		return 21;

	ua.value = a;
	ua.value += 3.0L;
	if (ua.parts[0] != 4.25L)
		return 22;
	if (ua.parts[1] != 3.75L)
		return 23;

	ub.value = a;
	ub.value *= (_Imaginary double)2.0;
	if (ub.parts[0] != -7.5L)
		return 24;
	if (ub.parts[1] != 2.5L)
		return 25;

	if (!(a == (_Imaginary long double)3.75L))
		return 26;
	if (a != (_Imaginary long double)3.75L)
		return 27;

	ua.value = complex_long_double_identity(a);
	if (ua.parts[0] != 1.25L)
		return 28;
	if (ua.parts[1] != 3.75L)
		return 29;

	{
		_Complex long double (*fp)(_Complex long double) = complex_long_double_identity;
		ub.value = fp(a);
	}
	if (ub.parts[0] != 1.25L)
		return 30;
	if (ub.parts[1] != 3.75L)
		return 31;

	uc.value = complex_long_double_outer(a);
	if (uc.parts[0] != 1.25L)
		return 32;
	if (uc.parts[1] != 3.75L)
		return 33;

	ud.value = a / (_Imaginary double)2.0;
	if (ud.parts[0] != 1.875L)
		return 34;
	if (ud.parts[1] != -0.625L)
		return 35;

	ua.value = (_Imaginary double)2.0 / a;
	if (ua.parts[0] != 0.48L)
		return 36;
	if (ua.parts[1] != 0.16L)
		return 37;

	ub.value = 0 ? a : (_Imaginary double)3.0;
	if (ub.parts[0] != 0.0L)
		return 38;
	if (ub.parts[1] != 3.0L)
		return 39;

	uc.value = 1 ? a : 4.0;
	if (uc.parts[0] != 1.25L)
		return 40;
	if (uc.parts[1] != 3.75L)
		return 41;

	return 48;
}
