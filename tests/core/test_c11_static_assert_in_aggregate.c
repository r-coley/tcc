struct S {
	int x;
	_Static_assert(sizeof(int) == 4, "int size");
	int y;
};

union U {
	int x;
	_Static_assert(sizeof(long) == 8, "long size");
	long y;
};

int
main(void)
{
	struct S s;
	union U u;

	s.x = 1;
	s.y = 2;
	u.x = 42;

	if (s.x + s.y != 3)
		return 1;
	if (u.x != 42)
		return 2;
	return 42;
}
