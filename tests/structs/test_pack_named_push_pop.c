#include <stddef.h>

#define PACK_PUSH_BETA _Pragma("pack(push, beta, 2)")
#define PACK_POP_BETA _Pragma("pack(pop, beta)")

#pragma pack(1)
struct BasePacked {
	char c;
	int i;
};

#pragma pack(push, alpha, 4)
struct AlphaPacked {
	char c;
	int i;
};

PACK_PUSH_BETA
struct BetaPacked {
	char c;
	int i;
};

PACK_POP_BETA
struct AfterBetaPacked {
	char c;
	int i;
};

#pragma pack(pop, alpha)
struct RestoredPacked {
	char c;
	int i;
};

#pragma pack()

int
main(void)
{
	int fail = 0;

	if (offsetof(struct BasePacked, i) != 1) fail++;
	if (offsetof(struct AlphaPacked, i) != 4) fail++;
	if (offsetof(struct BetaPacked, i) != 2) fail++;
	if (offsetof(struct AfterBetaPacked, i) != 4) fail++;
	if (offsetof(struct RestoredPacked, i) != 1) fail++;

	return fail;
}
