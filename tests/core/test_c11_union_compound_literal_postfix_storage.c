union U {
	int i;
	unsigned u;
};

static int *g_i = &((union U){ .i = 9 }).i;

int
main(void)
{
	if (*g_i != 9)
		return 1;
	*g_i = 12;
	if (*g_i != 12)
		return 2;
	return 12;
}
