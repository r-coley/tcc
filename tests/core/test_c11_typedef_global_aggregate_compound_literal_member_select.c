typedef struct {
	int x;
	int y;
} S;

typedef union {
	int i;
	unsigned u;
} U;

static int gs = ((S){ .x = 7, .y = 9 }).y;
static int gu = ((U){ .i = 11 }).i;

int
main(void)
{
	return (gs == 9 && gu == 11) ? 42 : 1;
}
