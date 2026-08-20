int
main(void)
{
	if (0)
		return 1;
	else
		_Static_assert(sizeof(int) == 4, "int size");
	return 42;
}
