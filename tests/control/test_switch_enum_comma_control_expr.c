enum Mode {
	MODE_ZERO = 0,
	MODE_ONE = 1,
	MODE_TWO = 2
};

int
main(void)
{
	int side_effect = 0;
	enum Mode mode = MODE_TWO;

	switch ((side_effect = 1, mode)) {
	case MODE_ZERO:
		return 1;
	case MODE_ONE:
		return 2;
	case MODE_TWO:
		return side_effect ? 42 : 3;
	default:
		return 4;
	}
}
