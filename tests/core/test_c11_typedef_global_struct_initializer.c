typedef struct {
	int x;
	int y;
} Pair;

typedef struct {
	Pair pair;
	int z;
} Wrap;

static Pair g_pair = { 1, 2 };
static Wrap g_wrap = { { 3, 4 }, 5 };

int
main(void)
{
	int total = 0;

	if (g_pair.x != 1 || g_pair.y != 2)
		return 1;
	if (g_wrap.pair.x != 3 || g_wrap.pair.y != 4 || g_wrap.z != 5)
		return 2;

	total += g_pair.x + g_pair.y;
	total += g_wrap.pair.x + g_wrap.pair.y + g_wrap.z;

	return total == 15 ? 42 : total;
}
