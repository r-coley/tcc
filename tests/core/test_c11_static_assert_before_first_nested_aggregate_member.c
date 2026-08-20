struct Outer {
	struct Inner {
		_Static_assert(sizeof(int) == 4, "ok");
		int x;
	} in;
};

int
main(void)
{
	struct Outer o;

	o.in.x = 42;
	return o.in.x;
}
