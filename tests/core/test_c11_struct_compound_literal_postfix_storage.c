struct Inner {
	int y;
};

struct Outer {
	int x;
	struct Inner inner;
};

static int *g_x = &((struct Outer){ .x = 3, .inner = { .y = 7 } }).x;
static int *g_y = &((struct Outer){ .x = 3, .inner = { .y = 7 } }).inner.y;

int
main(void)
{
	int total = 0;

	if (*g_x != 3)
		return 1;
	if (*g_y != 7)
		return 2;

	*g_x = 10;
	*g_y = 11;

	if (*g_x != 10)
		return 3;
	if (*g_y != 11)
		return 4;

	total += *g_x;
	total += *g_y;
	return total == 21 ? 21 : total;
}
