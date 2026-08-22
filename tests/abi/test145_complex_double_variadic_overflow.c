#include <stdarg.h>

int
check_complex_double_variadic_overflow(double a0, double a1, double a2,
                                       double a3, double a4, double a5,
                                       double a6, double a7, ...)
{
	va_list ap;
	_Complex double value;
	double *parts;

	(void)a0;
	(void)a1;
	(void)a2;
	(void)a3;
	(void)a4;
	(void)a5;
	(void)a6;
	(void)a7;
	va_start(ap, a7);
	value = va_arg(ap, _Complex double);
	va_end(ap);
	parts = (double *)&value;
	return parts[0] == 9.5 && parts[1] == 10.5 ? 42 : 1;
}
