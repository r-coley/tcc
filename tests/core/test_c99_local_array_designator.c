int
main(void)
{
	enum { FIRST = 1, LAST = 3 };
	int values[5] = { [2] = 40, [FIRST] = 1, [LAST] = 1, [LAST + 1] = 0 };

	return values[0] == 0 &&
	       values[1] == 1 &&
	       values[2] == 40 &&
	       values[3] == 1 &&
	       values[4] == 0 ? 42 : 1;
}
