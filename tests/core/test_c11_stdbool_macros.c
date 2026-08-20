#include <stdbool.h>

int
main(void)
{
	bool b = 7;

	if (b != true)
		return 1;
	if (false != 0)
		return 2;
	if (sizeof(b) != sizeof(_Bool))
		return 3;
#ifndef __bool_true_false_are_defined
	return 4;
#endif

	return 42;
}
