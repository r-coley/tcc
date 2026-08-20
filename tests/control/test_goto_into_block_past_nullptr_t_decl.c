#include <stddef.h>

int
main(void)
{
	int result = 1;

	goto inside;
	{
		nullptr_t p = nullptr;
		(void)p;
inside:
		result = 42;
	}

	return result;
}
