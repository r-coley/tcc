#include <stddef.h>

nullptr_t global_ptr = nullptr;

int
main(void)
{
	nullptr_t local_ptr = nullptr;
	if (global_ptr != 0)
		return 1;
	if (local_ptr != 0)
		return 1;
	return 42;
}
