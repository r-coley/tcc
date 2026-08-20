#include <stddef.h>

#define PACK_POP_BETA_SET4 _Pragma("pack(pop, beta, 4)")

#pragma pack(1)
struct Base1 {
	char c;
	int i;
};

#pragma pack(push, alpha, 8)
struct Alpha8 {
	char c;
	int i;
};

#pragma pack(push, beta, 2)
struct Beta2 {
	char c;
	int i;
};

#pragma pack(pop, 4)
struct AfterPop4 {
	char c;
	int i;
};

#pragma pack(push, beta, 2)
struct Beta2Again {
	char c;
	int i;
};

PACK_POP_BETA_SET4
struct AfterNamedPop4 {
	char c;
	int i;
};

#pragma pack(pop, alpha)
struct AfterAlphaPop1 {
	char c;
	int i;
};

#pragma pack()

int
main(void)
{
	int fail = 0;

	if (offsetof(struct Base1, i) != 1) fail++;
	if (offsetof(struct Alpha8, i) != 4) fail++;
	if (offsetof(struct Beta2, i) != 2) fail++;
	if (offsetof(struct AfterPop4, i) != 4) fail++;
	if (offsetof(struct Beta2Again, i) != 2) fail++;
	if (offsetof(struct AfterNamedPop4, i) != 4) fail++;
	if (offsetof(struct AfterAlphaPop1, i) != 1) fail++;

	return fail;
}
