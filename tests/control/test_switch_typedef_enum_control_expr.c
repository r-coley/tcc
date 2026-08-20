typedef enum {
	COLOR_RED = 1,
	COLOR_GREEN = 2,
	COLOR_BLUE = 3
} Color;

int
main(void)
{
	Color c = COLOR_GREEN;

	switch (c) {
	case COLOR_RED:
		return 1;
	case COLOR_GREEN:
		return 42;
	case COLOR_BLUE:
		return 2;
	default:
		return 0;
	}
}
