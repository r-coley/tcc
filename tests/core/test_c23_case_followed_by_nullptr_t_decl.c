#include <stddef.h>

int
main(void)
{
	switch (2) {
	case 2:
		nullptr_t p = nullptr;
		return p == nullptr ? 42 : 0;
	default:
		return 0;
	}
}
