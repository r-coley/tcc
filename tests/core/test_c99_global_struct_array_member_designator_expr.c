struct S {
	int values[5];
};

enum {
	FIRST = 1,
	LAST = 3
};

struct S s = { .values = { [FIRST ... LAST] = 7, [LAST + 1] = 8 } };

int
main(void)
{
	return s.values[0] == 0 &&
	       s.values[1] == 7 &&
	       s.values[2] == 7 &&
	       s.values[3] == 7 &&
	       s.values[4] == 8 ? 42 : 1;
}
