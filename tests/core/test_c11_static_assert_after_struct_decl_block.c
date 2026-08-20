int
main(void)
{
	struct S {
		int x;
	};

	_Static_assert(sizeof(struct S) == sizeof(int), "ok");
	return 42;
}
