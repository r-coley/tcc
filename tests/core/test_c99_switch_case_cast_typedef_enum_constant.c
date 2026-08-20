typedef enum {
	COLOR_RED = 1,
	COLOR_GREEN = 2
} Color;

int
main(void)
{
	int x = 2;

	switch (x) {
	case (Color)COLOR_RED:
		return 1;
	case (Color)COLOR_GREEN:
		return 42;
	default:
		return 2;
	}
}
