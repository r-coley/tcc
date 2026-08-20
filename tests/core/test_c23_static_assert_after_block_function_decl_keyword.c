int
main(void)
{
	int helper(void);
	static_assert(sizeof(int) == 4, "int size");
	return 42;
}
