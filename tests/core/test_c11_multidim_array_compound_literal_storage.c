static int (*g_mat)[2] = (int[2][2]){ { 1, 2 }, { 3, 4 } };

int
main(void)
{
	int total = 0;

	if (g_mat[0][0] != 1 || g_mat[0][1] != 2 || g_mat[1][0] != 3 || g_mat[1][1] != 4)
		return 1;
	if (g_mat[1][0] != 3 || g_mat[1][1] != 4)
		return 2;

	g_mat[1][0] = 9;
	g_mat[1][1] = 10;

	if (g_mat[1][0] != 9)
		return 3;
	if (g_mat[1][1] != 10)
		return 4;

	total += g_mat[0][0];
	total += g_mat[0][1];
	total += g_mat[1][0];
	total += g_mat[1][1];

	return total == 22 ? 22 : total;
}
