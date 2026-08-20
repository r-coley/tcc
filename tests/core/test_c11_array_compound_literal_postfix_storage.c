static int *g_elem = &(int[]){ 4, 5, 6 }[1];
static int (*g_row)[2] = &(int[2][2]){ { 1, 2 }, { 3, 4 } }[1];

int
main(void)
{
	int total = 0;

	if (*g_elem != 5)
		return 1;
	if ((*g_row)[0] != 3 || (*g_row)[1] != 4)
		return 2;

	*g_elem = 8;
	(*g_row)[1] = 7;

	if (*g_elem != 8)
		return 3;
	if ((*g_row)[1] != 7)
		return 4;

	total += *g_elem;
	total += (*g_row)[0];
	total += (*g_row)[1];

	return total == 18 ? 18 : total;
}
