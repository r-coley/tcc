#include <stdarg.h>

static int
check_va_copy(int tag, ...)
{
	va_list ap;
	va_list copy;

	va_start(ap, tag);
	va_copy(copy, ap);
	if (va_arg(copy, int) != 42)
		return 1;
	if (va_arg(ap, int) != 42)
		return 2;
	va_end(copy);
	va_end(ap);
	return tag == 7 ? 42 : 3;
}

int
main(void)
{
	return check_va_copy(7, 42);
}
