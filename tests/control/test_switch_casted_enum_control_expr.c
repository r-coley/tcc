enum Mode {
	MODE_ZERO = 0,
	MODE_ONE = 1,
	MODE_TWO = 2
};

int
main(void)
{
	int raw = 1;

	switch ((enum Mode)raw) {
	case MODE_ZERO:
		return 1;
	case MODE_ONE:
		return 42;
	case MODE_TWO:
		return 2;
	default:
		return 3;
	}
}
