int
main(void)
{
	switch (1) {
	case 1:
		_Static_assert(sizeof(int) == 4, "int size");
		return 42;
	default:
		return 1;
	}
}
