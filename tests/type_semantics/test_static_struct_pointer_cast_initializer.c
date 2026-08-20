#include <stdint.h>

typedef struct Box Box;

struct Box {
	void *p;
};

int
main(void)
{
	static Box boxes[] = {
		{ ((void *)(intptr_t)1) },
		{ ((void *)(intptr_t)7) },
	};

	if( (intptr_t)boxes[0].p != 1 )
		return 10;
	if( (intptr_t)boxes[1].p != 7 )
		return 11;
	return 42;
}
