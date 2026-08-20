int
main(void)
{
	switch (0) {
	case 1:
		return 0;
	default:
		register int x = 42;
		return x;
	}
}
