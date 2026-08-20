struct Point {
	int x;
	int y;
};

static struct Point g_obj = (struct Point){ .x = 7, .y = 11 };
static struct Point *g_ptr_a = &(struct Point){ .x = 13, .y = 17 };
static struct Point *g_ptr_b = &((struct Point){ .y = 19, .x = 23 });

static int
read_point(struct Point p)
{
	return p.x + p.y;
}

int
main(void)
{
	int total = 0;

	if (g_obj.x != 7 || g_obj.y != 11)
		return 1;
	if (g_ptr_a->x != 13 || g_ptr_a->y != 17)
		return 2;
	if (g_ptr_b->x != 23 || g_ptr_b->y != 19)
		return 3;

	g_ptr_a->x = 2;
	g_ptr_b->y = 1;

	if (g_ptr_a->x != 2 || g_ptr_a->y != 17)
		return 4;
	if (g_ptr_b->x != 23 || g_ptr_b->y != 1)
		return 5;

	total += read_point(g_obj);
	total += g_ptr_a->x + g_ptr_a->y;
	total += g_ptr_b->x + g_ptr_b->y;

	return total == 61 ? 42 : total;
}
