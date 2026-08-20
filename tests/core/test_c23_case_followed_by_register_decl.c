int
main(void)
{
	switch (2) {
	case 2:
		register int x = 42;
		return x;
	default:
		return 0;
	}
}
