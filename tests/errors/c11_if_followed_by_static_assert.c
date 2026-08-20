int
main(void)
{
	if (1)
		_Static_assert(sizeof(int) == 4, "int size");
	return 42;
}
