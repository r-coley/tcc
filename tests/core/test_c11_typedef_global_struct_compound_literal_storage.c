typedef struct {
	int x;
	int y;
} S;

static S g_value = (S){ 10, 20 };
static S *g_ptr = &(S){ 7, 8 };

int
main(void)
{
	if (g_value.x != 10 || g_value.y != 20)
		return 1;
	if (g_ptr->x != 7 || g_ptr->y != 8)
		return 2;

	g_ptr->x = 11;
	g_ptr->y = 12;
	if (g_ptr->x != 11 || g_ptr->y != 12)
		return 3;

	return g_value.x + g_value.y + g_ptr->x + g_ptr->y == 53 ? 42 : 4;
}
