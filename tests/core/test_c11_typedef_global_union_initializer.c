typedef union {
	int i;
	unsigned u;
} Value;

static Value g_value = { .i = 9 };

int
main(void)
{
	if (g_value.i != 9)
		return 1;

	g_value.i = 12;
	if (g_value.i != 12)
		return 2;

	return 42;
}
