static int helper(void);

_Static_assert(sizeof(int) == 4, "int size");

static int
helper(void)
{
	return 40;
}

int
main(void)
{
	return helper() + 2;
}
