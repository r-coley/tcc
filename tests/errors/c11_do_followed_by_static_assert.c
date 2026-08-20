int
main(void)
{
	do
		_Static_assert(sizeof(int) == 4, "int size");
	while (0);
	return 42;
}
