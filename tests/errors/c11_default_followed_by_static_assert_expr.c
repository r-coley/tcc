int
main(void)
{
	switch (1) {
	case 0:
		return 1;
	default:
		_Static_assert(sizeof(int) == 4, "int size");
		return 42;
	}
}
