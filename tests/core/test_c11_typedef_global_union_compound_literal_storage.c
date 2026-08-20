typedef union {
	int i;
	unsigned u;
} U;

static U g_value = (U){ 11 };
static U *g_ptr = &(U){ 31 };

int
main(void)
{
	if (g_value.i != 11)
		return 1;
	if (g_ptr->i != 31)
		return 2;

	return 42;
}
