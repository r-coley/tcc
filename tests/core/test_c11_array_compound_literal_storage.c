static int *g_arr = (int[]){ 3, 4, 5 };
static int (*g_arrp)[3] = &(int[3]){ 6, 7, 8 };

int
main(void)
{
	int total = 0;

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

	total += g_arr[0];
	total += g_arr[1];
	total += g_arr[2];
	total += (*g_arrp)[0];
	total += (*g_arrp)[1];
	total += (*g_arrp)[2];

	return total == 40 ? 40 : total;
}
