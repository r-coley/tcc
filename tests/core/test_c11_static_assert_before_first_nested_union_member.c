struct S {
	union U {
		_Static_assert(sizeof(long) == 8, "ok");
		long y;
	} u;
};

int
main(void)
{
	struct S s;

	s.u.y = 42;
	return s.u.y;
}
