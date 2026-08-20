enum Color {
	COLOR_RED,
	COLOR_BLUE = 5,
	COLOR_GREEN,
	COLOR_MASK = COLOR_BLUE | 8,
	COLOR_SHIFT = 1 << 4,
	COLOR_NEG = -3
};

enum Limits {
	ENUM_INT_MAX = 2147483647,
	ENUM_INT_MIN = -2147483647 - 1
};

int
main(void)
{
	enum Color c = COLOR_BLUE;

	if (COLOR_RED != 0)
		return 1;
	if (COLOR_GREEN != 6)
		return 2;
	if (COLOR_MASK != 13)
		return 3;
	if (COLOR_SHIFT != 16)
		return 4;
	if (COLOR_NEG != -3)
		return 5;
	if (ENUM_INT_MAX != 2147483647)
		return 6;
	if (ENUM_INT_MIN != (-2147483647 - 1))
		return 7;
	if (sizeof(COLOR_BLUE) != sizeof(int))
		return 8;
	if ((c + 1) != 6)
		return 9;
	if (sizeof(+c) != sizeof(int))
		return 10;

	{
		enum { COLOR_BLUE = 22 };
		if (COLOR_BLUE != 22)
			return 11;
	}
	if (COLOR_BLUE != 5)
		return 12;

	return 42;
}
