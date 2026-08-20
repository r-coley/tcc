enum Mode {
	MODE_ZERO = 0,
	MODE_ONE = 1,
	MODE_TWO = 2
};

int
main(void)
{
	volatile int choose_first = 1;

	switch (choose_first ? (enum Mode)MODE_ONE : (enum Mode)MODE_TWO) {
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
