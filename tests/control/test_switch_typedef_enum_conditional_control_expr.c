typedef enum {
	COLOR_RED = 1,
	COLOR_GREEN = 2,
	COLOR_BLUE = 3
} Color;

int
main(void)
{
	volatile int choose_green = 1;

	switch (choose_green ? COLOR_GREEN : COLOR_BLUE) {
	case COLOR_RED:
		return 1;
	case COLOR_GREEN:
		return 42;
	case COLOR_BLUE:
		return 2;
	default:
		return 3;
	}
}
