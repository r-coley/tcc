int helper(void);

static_assert(sizeof(int) == 4, "int size");

int
helper(void)
{
	return 42;
}

int
main(void)
{
	return helper();
}
