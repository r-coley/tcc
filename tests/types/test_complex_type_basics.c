typedef _Complex double complex_double_t;

struct S {
	_Complex float a;
	_Complex double b;
};

static _Complex double global_value;

int
main(void)
{
	_Complex float local_float;
	_Complex double local_double;
	complex_double_t *ptr = &global_value;

	if (sizeof(_Complex float) != 8)
		return 1;
	if (sizeof(_Complex double) != 16)
		return 3;
	if (sizeof(complex_double_t) != 16)
		return 5;
	if (sizeof(global_value) != 16)
		return 6;
	if (sizeof(struct S) != 24)
		return 7;
	if (sizeof(local_float) != 8)
		return 8;
	if (sizeof(local_double) != 16)
		return 9;
	if (sizeof(*ptr) != 16)
		return 10;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
	if (_Alignof(_Complex float) != 4)
		return 2;
	if (_Alignof(_Complex double) != 8)
		return 4;
#endif
	return 42;
}
