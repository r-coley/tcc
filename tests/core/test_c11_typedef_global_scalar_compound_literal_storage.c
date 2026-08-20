typedef int I;

static I g_value = (I){ 7 };
static I *g_ptr = &(I){ 9 };

int
main(void)
{
	if (g_value != 7)
		return 1;
	if (*g_ptr != 9)
		return 2;

	*g_ptr = 26;
	if (*g_ptr != 26)
		return 3;

	return g_value + *g_ptr == 33 ? 42 : 4;
}
