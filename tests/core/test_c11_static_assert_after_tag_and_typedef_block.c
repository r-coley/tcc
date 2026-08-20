int
main(void)
{
	struct S {
		int x;
	};
	typedef struct S S;

	_Static_assert(sizeof(S) == sizeof(int), "ok");
	return 42;
}
