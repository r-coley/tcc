static int
helper(void)
{
	return 40;
}

static_assert(sizeof(int) == 4, "int size");

int
main(void)
{
	return helper() + 2;
}
