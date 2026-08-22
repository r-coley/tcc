#include <stdarg.h>

static int
check_complex_tail(int tag, ...)
{
	va_list ap;
	_Complex double value;
	double *parts;

	va_start(ap, tag);
	value = va_arg(ap, _Complex double);
	va_end(ap);

	parts = (double *)&value;
	if (tag != 7)
		return 1;
	if (parts[0] != 1.5)
		return 2;
	if (parts[1] != 2.5)
		return 3;
	return 42;
}

int
main(void)
{
	_Complex double value;
	double *parts = (double *)&value;

	parts[0] = 1.5;
	parts[1] = 2.5;
	return check_complex_tail(7, value);
}
