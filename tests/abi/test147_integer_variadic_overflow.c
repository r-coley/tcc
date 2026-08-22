#include <stdarg.h>

static int
check_integer_variadic_overflow(int a0, int a1, int a2, int a3, int a4, int a5,
                                ...)
{
	va_list ap;
	int value;

	(void)a0;
	(void)a1;
	(void)a2;
	(void)a3;
	(void)a4;
	(void)a5;
	va_start(ap, a5);
	value = va_arg(ap, int);
	va_end(ap);
	return value == 42 ? 42 : 1;
}

int
main(void)
{
	return check_integer_variadic_overflow(1, 2, 3, 4, 5, 6, 42);
}
