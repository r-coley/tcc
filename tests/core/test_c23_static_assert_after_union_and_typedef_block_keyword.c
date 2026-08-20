int
main(void)
{
	union U {
		int x;
	};
	typedef union U U;

	static_assert(sizeof(U) == sizeof(int), "ok");
	return 42;
}
