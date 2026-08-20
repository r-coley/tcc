int
main(void)
{
	struct S {
		int x;
	};
	typedef struct S S;

	static_assert(sizeof(S) == sizeof(int), "ok");
	return 42;
}
