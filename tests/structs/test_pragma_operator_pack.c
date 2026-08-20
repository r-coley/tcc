#include <stddef.h>

#define PACK1 _Pragma("pack(1)")
#define PACKRESET _Pragma("pack()")

PACK1
struct PackedGlobal {
	char c;
	int i;
	long l;
};
PACKRESET

int
main(void)
{
	int fail = 0;

	if (offsetof(struct PackedGlobal, c) != 0) fail++;
	if (offsetof(struct PackedGlobal, i) != 1) fail++;
	if (offsetof(struct PackedGlobal, l) != 5) fail++;

	PACK1
	struct PackedLocal {
		char c;
		int i;
		long l;
	};
	PACKRESET

	if (offsetof(struct PackedLocal, c) != 0) fail++;
	if (offsetof(struct PackedLocal, i) != 1) fail++;
	if (offsetof(struct PackedLocal, l) != 5) fail++;

	return fail;
}
