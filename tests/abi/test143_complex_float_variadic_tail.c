#include <stdarg.h>

static int
check_complex_float_tail(int tag, ...)
{
	va_list ap;
	_Complex float value;
	float *parts;

	va_start(ap, tag);
	value = va_arg(ap, _Complex float);
	va_end(ap);
	parts = (float *)&value;

	if (tag != 7)
		return 1;
	if (parts[0] != 1.5f || parts[1] != 2.5f)
		return 2;
	return 42;
}

int
main(void)
{
	_Complex float value;
	float *parts = (float *)&value;

	parts[0] = 1.5f;
	parts[1] = 2.5f;
	return check_complex_float_tail(7, value);
}
