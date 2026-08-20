#include <stddef.h>

#define PACK1 _Pragma("pack(1)" \
)
#define PACKRESET _Pragma("pack()")

PACK1
struct PackedGlobalSplice {
	char c;
	int i;
	long l;
};
PACKRESET

int
main(void)
{
	int fail = 0;

	if (offsetof(struct PackedGlobalSplice, c) != 0) fail++;
	if (offsetof(struct PackedGlobalSplice, i) != 1) fail++;
	if (offsetof(struct PackedGlobalSplice, l) != 5) fail++;

	return fail;
}
