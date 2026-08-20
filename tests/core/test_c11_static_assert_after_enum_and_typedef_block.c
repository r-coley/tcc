int
main(void)
{
	enum E {
		A = 42
	};
	typedef enum E E;

	_Static_assert(A == 42, "ok");
	_Static_assert(sizeof(E) == sizeof(int), "enum typedef size");
	return 42;
}
