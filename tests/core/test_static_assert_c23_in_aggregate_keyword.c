struct S {
	static_assert(sizeof(int) == 4);
	int x;
};

union U {
	static_assert(sizeof(long) == 8, "long size");
	long y;
};

int
main(void)
{
	struct S s = { 40 };
	union U u = { 2 };

	return s.x + (int)u.y;
}
