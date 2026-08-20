int
main(void)
{
	union U {
		long y;
	};

	_Static_assert(sizeof(union U) == sizeof(long), "ok");
	return 42;
}
