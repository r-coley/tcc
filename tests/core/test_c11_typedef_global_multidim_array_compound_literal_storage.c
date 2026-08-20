typedef int A22[2][2];

static int (*g_row)[2] = (A22){ { 1, 2 }, { 3, 4 } };
static A22 *g_arrp = &(A22){ { 5, 6 }, { 7, 8 } };

int
main(void)
{
	if (g_row[0][0] != 1 || g_row[0][1] != 2 ||
	    g_row[1][0] != 3 || g_row[1][1] != 4)
		return 1;
	if ((*g_arrp)[0][1] != 6 || (*g_arrp)[1][0] != 7)
		return 2;

	g_row[1][0] = 9;
	(*g_arrp)[1][1] = 10;

	if (g_row[1][0] != 9)
		return 3;
	if ((*g_arrp)[1][1] != 10)
		return 4;

	return g_row[0][0] + g_row[0][1] + g_row[1][0] + g_row[1][1] +
	           (*g_arrp)[0][0] + (*g_arrp)[0][1] + (*g_arrp)[1][0] + (*g_arrp)[1][1] == 44
	           ? 42 : 5;
}
