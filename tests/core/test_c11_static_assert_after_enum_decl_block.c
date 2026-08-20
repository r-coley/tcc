int
main(void)
{
	enum E {
		A = 1
	};

	_Static_assert(A == 1, "ok");
	return 42;
}
