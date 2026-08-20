enum Mode {
	MODE_ZERO = 0,
	MODE_ONE = 1,
	MODE_TWO = 2
};

int
main(void)
{
	int raw = MODE_ONE;

	switch ((enum Mode)raw + 1) {
	case MODE_ZERO:
		return 1;
	case MODE_ONE:
		return 2;
	case MODE_TWO:
		return 42;
	default:
		return 3;
	}
}
