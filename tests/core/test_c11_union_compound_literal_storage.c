union Number {
	int i;
	unsigned u;
};

static union Number g_obj = (union Number){ .i = 7 };
static union Number *g_ptr_a = &(union Number){ .i = 11 };
static union Number *g_ptr_b = &((union Number){ .u = 13U });

static int
read_union(union Number n)
{
	return n.i;
}

int
main(void)
{
	int total = 0;

	if (g_obj.i != 7)
		return 1;
	if (g_ptr_a->i != 11)
		return 2;
	if (g_ptr_b->u != 13U)
		return 3;

	g_ptr_a->i = 5;
	g_ptr_b->u = 6U;

	if (g_ptr_a->i != 5)
		return 4;
	if (g_ptr_b->u != 6U)
		return 5;

	total += read_union(g_obj);
	total += g_ptr_a->i;
	total += g_ptr_b->u;
	total += ((union Number){ .i = 24 }).i;

	return total == 42 ? 42 : total;
}
