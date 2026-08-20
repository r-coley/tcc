static int
helper(void)
{
	return 40;
}

_Static_assert(sizeof(int) == 4, "int size");

int
main(void)
{
	return helper() + 2;
}
