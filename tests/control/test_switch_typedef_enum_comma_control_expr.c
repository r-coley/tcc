typedef enum {
	COLOR_RED = 1,
	COLOR_GREEN = 2,
	COLOR_BLUE = 3
} Color;

int
main(void)
{
	int side_effect = 0;
	Color color = COLOR_BLUE;

	switch ((side_effect = 1, color)) {
	case COLOR_RED:
		return 1;
	case COLOR_GREEN:
		return 2;
	case COLOR_BLUE:
		return side_effect ? 42 : 3;
	default:
		return 4;
	}
}
