#include <stddef.h>

int
main(void)
{
	switch (0) {
	case 1:
		return 0;
	default:
		nullptr_t p = nullptr;
		return p == nullptr ? 42 : 0;
	}
}
