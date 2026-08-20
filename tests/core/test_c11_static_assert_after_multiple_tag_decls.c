int
main(void)
{
	struct A;
	union B;
	enum C {
		C0 = 1
	};

	_Static_assert(C0 == 1, "ok");
	return 42;
}
