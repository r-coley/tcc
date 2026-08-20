#include <stddef.h>

int
main(void)
{
	int value = 4;
	__typeof__(value) same = value + 3;
	typeof(int *) ptr = &value;
	typeof(*ptr) copy = *ptr;

	if (same != 7)
		return 1;
	if (copy != 4)
		return 2;
	if ((size_t)sizeof(typeof(value)) != sizeof(int))
		return 3;
	if ((size_t)sizeof(__typeof__(*ptr)) != sizeof(int))
		return 4;

	return 42;
}
