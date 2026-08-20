int
main(void)
{
	int mode = 0;

	goto start;

switch_case:
	switch (1) {
	case 1:
		static_assert(sizeof(int) == 4);
		break;
	default:
		return 1;
	}

	switch (mode) {
	default:
		static_assert(sizeof(long) == 8, "long size");
		return 42;
	}

start:
	static_assert(sizeof(char) == 1);
	goto switch_case;
}
