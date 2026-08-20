enum {
	WIDTH_THREE = 1 + 2,
	WIDTH_FOUR = sizeof(int)
};

struct S {
	unsigned int a : WIDTH_THREE;
	unsigned int b : WIDTH_FOUR;
	unsigned int c : sizeof(short) == 2 ? 5 : 1;
};

int
main(void)
{
	struct S s;

	s.a = 5;
	s.b = 15;
	s.c = 31;

	return s.a + s.b + s.c - 9;
}
