#define TYPEOF(x) _Generic((x), int: 1, enum Color: 2, default: 99)

enum Color {
	COLOR_RED,
	COLOR_BLUE = 5
};

int
main(void)
{
	enum Color c = COLOR_BLUE;

	if (TYPEOF(COLOR_BLUE) != 1)
		return 1;
	if (TYPEOF(c) != 2)
		return 2;
	if (TYPEOF(+c) != 1)
		return 3;

	return 42;
}
