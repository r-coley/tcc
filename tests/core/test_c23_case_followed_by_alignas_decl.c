int
main(void)
{
	switch (2) {
	case 2:
		alignas(16) int x = 42;
		return x;
	default:
		return 0;
	}
}
