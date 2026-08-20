typedef int (*variadic_inner_a)(int, ...);
typedef int (*variadic_inner_b)(int, ...);
typedef variadic_inner_a (*variadic_outer_a)(void);
typedef variadic_inner_b (*variadic_outer_b)(void);

struct holder {
	variadic_outer_b fp;
};

static int
add2(int x, ...)
{
	return x + 2;
}

static variadic_inner_a
make_inner(void)
{
	return add2;
}

variadic_outer_a maker;
variadic_outer_b maker = make_inner;

extern variadic_outer_a ext_maker;
variadic_outer_b ext_maker = make_inner;

int
main(void)
{
	variadic_outer_b local = maker;
	variadic_outer_b arr[2] = { [1] = maker };
	struct holder h = { maker };
	int pick = 1;
	variadic_outer_a fallback = maker;

	if (maker()(40, 7) != 42)
		return 1;
	if (ext_maker()(40, 7) != 42)
		return 2;
	if (local()(40, 7) != 42)
		return 3;
	if (arr[1]()(40, 7) != 42)
		return 4;
	if (h.fp()(40, 7) != 42)
		return 5;
	if ((pick ? maker : fallback)()(40, 7) != 42)
		return 6;
	if ((0 ? fallback : maker)()(40, 7) != 42)
		return 7;

	return 42;
}
