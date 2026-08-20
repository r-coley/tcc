int
main(void)
{
	int x = 0;

	switch (x) {
	case (1 ? 0 : 1):
		return 42;
	default:
		return 0;
	}
}
