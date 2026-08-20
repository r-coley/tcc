int
main(void)
{
	int x = 'B';

	switch (x) {
	case (int)'A':
		return 1;
	case (int)'B':
		return 42;
	default:
		return 2;
	}
}
