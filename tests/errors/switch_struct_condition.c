struct Pair {
	int a;
	int b;
};

int
main(void)
{
	struct Pair pair = { 1, 2 };

	switch (pair) {
	case 0:
		return 1;
	default:
		return 0;
	}
}
