typedef enum {
	COLOR_RED = 1,
	COLOR_GREEN = 2,
	COLOR_BLUE = 3
} Color;

int
main(void)
{
	Color color = COLOR_GREEN;

	switch (color + 1) {
	case COLOR_RED:
		return 1;
	case COLOR_GREEN:
		return 2;
	case COLOR_BLUE:
		return 42;
	default:
		return 3;
	}
}
