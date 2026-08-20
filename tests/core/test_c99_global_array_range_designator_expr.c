enum {
	FIRST = 1,
	LAST = 3
};

int values[5] = { [FIRST ... LAST] = 7, [LAST + 1] = 8 };

int
main(void)
{
	return values[0] == 0 &&
	       values[1] == 7 &&
	       values[2] == 7 &&
	       values[3] == 7 &&
	       values[4] == 8 ? 42 : 1;
}
