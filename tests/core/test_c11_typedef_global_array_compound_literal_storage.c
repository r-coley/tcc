typedef int A3[3];

static int *g_arr = (A3){ 3, 4, 5 };
static A3 *g_arrp = &(A3){ 6, 7, 8 };

int
main(void)
{
	if (g_arr[0] != 3 || g_arr[1] != 4 || g_arr[2] != 5)
		return 1;
	if ((*g_arrp)[0] != 6 || (*g_arrp)[1] != 7 || (*g_arrp)[2] != 8)
		return 2;

	g_arr[1] = 9;
	(*g_arrp)[2] = 10;

	if (g_arr[1] != 9)
		return 3;
	if ((*g_arrp)[2] != 10)
		return 4;

	return g_arr[0] + g_arr[1] + g_arr[2] +
	           (*g_arrp)[0] + (*g_arrp)[1] + (*g_arrp)[2] == 40 ? 42 : 5;
}
