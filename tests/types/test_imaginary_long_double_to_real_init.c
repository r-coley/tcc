#include <stddef.h>

int
main(void)
{
	_Imaginary long double y = (_Imaginary long double)3.5L;
	long double x = y;

	if (x != 0.0L)
		return 1;

	x = (_Imaginary long double)2.0L;
	if (x != 0.0L)
		return 2;

	return 42;
}
