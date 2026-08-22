#include <stdarg.h>

int
check_complex_float_variadic_overflow(float a0, float a1, float a2, float a3,
                                      float a4, float a5, float a6, float a7,
                                      ...)
{
	va_list ap;
	_Complex float value;
	float *parts;

	(void)a0;
	(void)a1;
	(void)a2;
	(void)a3;
	(void)a4;
	(void)a5;
	(void)a6;
	(void)a7;
	va_start(ap, a7);
	value = va_arg(ap, _Complex float);
	va_end(ap);
	parts = (float *)&value;
	return parts[0] == 11.5f && parts[1] == 12.5f ? 42 : 1;
}
