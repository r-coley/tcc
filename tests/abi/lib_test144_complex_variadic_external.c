#include <stdarg.h>

int
consume_complex_variadic_external(int tag, ...)
{
	va_list ap;
	_Complex float first;
	_Complex double second;
	float *first_parts;
	double *second_parts;

	va_start(ap, tag);
	first = va_arg(ap, _Complex float);
	second = va_arg(ap, _Complex double);
	va_end(ap);
	first_parts = (float *)&first;
	second_parts = (double *)&second;

	if (tag != 7)
		return 1;
	if (first_parts[0] != 1.5f || first_parts[1] != 2.5f)
		return 2;
	if (second_parts[0] != 3.5 || second_parts[1] != 4.5)
		return 3;
	return 42;
}
