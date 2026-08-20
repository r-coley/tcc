int
main(void)
{
	union U {
		int x;
	};
	typedef union U U;

	_Static_assert(sizeof(U) == sizeof(int), "ok");
	return 42;
}
