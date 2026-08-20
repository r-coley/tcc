typedef int I;
typedef double D;

static I g_i = 40;
static D g_d = 2.0;

int
main(void)
{
	if (g_i != 40)
		return 1;
	if (g_d != 2.0)
		return 2;

	g_i += 1;
	g_d += 1.0;

	if (g_i != 41)
		return 3;
	if (g_d != 3.0)
		return 4;

	return g_i + (int)g_d == 44 ? 42 : 5;
}
