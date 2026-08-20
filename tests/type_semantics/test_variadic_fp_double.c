#include <stdarg.h>

static int
check_fp_tail(int tag, ...)
{
	va_list ap;
	double a;
	double b;

	va_start(ap, tag);
	a = va_arg(ap, double);
	b = va_arg(ap, double);
	va_end(ap);

	if (tag != 7)
		return 1;
	if (a != 1.5)
		return 2;
	if (b != 2.5)
		return 3;
	return 42;
}

int main(void)
{
	float f = 1.5f;

	return check_fp_tail(7, f, 2.5);
}
