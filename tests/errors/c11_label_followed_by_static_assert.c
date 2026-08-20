int
main(void)
{
label:
	_Static_assert(sizeof(int) == 4, "int size");
	return 42;
}
