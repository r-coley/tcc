typedef int A3[3];

static int *g_elem = &(A3){ 4, 5, 6 }[1];
static int (*g_arrp)[3] = &((A3){ 7, 8, 9 });

int
main(void)
{
	if (*g_elem != 5)
		return 1;
	if ((*g_arrp)[0] != 7 || (*g_arrp)[2] != 9)
		return 2;

	*g_elem = 10;
	(*g_arrp)[2] = 11;

	if (*g_elem != 10)
		return 3;
	if ((*g_arrp)[2] != 11)
		return 4;

	return 42;
}
