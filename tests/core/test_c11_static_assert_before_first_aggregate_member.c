struct S {
	_Static_assert(sizeof(int) == 4, "int size");
	int x;
};

union U {
	_Static_assert(sizeof(long) == 8, "long size");
	long y;
};

int
main(void)
{
	struct S s;
	union U u;

	s.x = 1;
	u.y = 41;

	return (s.x == 1 && u.y == 41) ? 42 : 1;
}
