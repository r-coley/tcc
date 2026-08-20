#include <stdalign.h>

alignas(16) int global_aligned;

int main(void)
{
	alignas(16) int local_aligned = 7;

	if (__alignas_is_defined != 1)
		return 1;
	if (__alignof_is_defined != 1)
		return 2;
	if (((unsigned long)&global_aligned & 15UL) != 0)
		return 3;
	if (((unsigned long)&local_aligned & 15UL) != 0)
		return 4;
	if (alignof(global_aligned) != 16)
		return 5;
	if (alignof(int) != 4)
		return 6;

	return 42;
}
