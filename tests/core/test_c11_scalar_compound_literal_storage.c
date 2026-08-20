static int g_value = (int){ 9 };
static int *g_ptr_a = &(int){ 13 };
static int *g_ptr_b = &((int){{ 21 }});

int
main(void)
{
	int total = 0;

	if (g_value != 9)
		return 1;
	if (*g_ptr_a != 13)
		return 2;
	if (*g_ptr_b != 21)
		return 3;

	*g_ptr_a = 4;
	*g_ptr_b = 8;

	if (*g_ptr_a != 4)
		return 4;
	if (*g_ptr_b != 8)
		return 5;

	total += g_value;
	total += *g_ptr_a;
	total += *g_ptr_b;

	return total == 21 ? 42 : total;
}
