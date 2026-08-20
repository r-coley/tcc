typedef int A4[4];

static int *g_elem = &((A4){ [2] = 40, [0] = 1 })[2];
static int (*g_arrp)[4] = &((A4){ [2] = 40, [0] = 1 });
static int g_val = ((A4){ [2] = 40, [0] = 1 })[0];

int
main(void)
{
	if (*g_elem != 40)
		return 1;
	if ((*g_arrp)[0] != 1 || (*g_arrp)[1] != 0 || (*g_arrp)[2] != 40)
		return 2;
	if (g_val != 1)
		return 3;
	return 42;
}
