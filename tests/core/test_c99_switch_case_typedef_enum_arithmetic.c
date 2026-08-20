typedef enum {
	COLOR_RED = 1,
	COLOR_GREEN = 2,
	COLOR_BLUE = 4
} Color;

int
main(void)
{
	Color color = COLOR_BLUE;

	switch (color) {
	case COLOR_RED | COLOR_GREEN:
		return 1;
	case COLOR_RED + COLOR_GREEN + 1:
		return 42;
	default:
		return 0;
	}
}
