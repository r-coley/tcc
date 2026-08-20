typedef int (*inner_a)(int);
typedef int (*inner_b)(int);
typedef inner_a (*outer_a)(void);
typedef inner_b (*outer_b)(void);

struct holder {
	outer_b fp;
};

static int
add2(int x)
{
	return x + 2;
}

static inner_a
make_inner(void)
{
	return add2;
}

outer_a maker;
outer_b maker = make_inner;

extern outer_a ext_maker;
outer_b ext_maker = make_inner;

int
main(void)
{
	outer_b local = maker;
	outer_b arr[2] = { [1] = maker };
	struct holder h = { maker };
	int pick = 1;
	outer_a fallback = maker;

	if (maker()(40) != 42)
		return 1;
	if (ext_maker()(40) != 42)
		return 2;
	if (local()(40) != 42)
		return 3;
	if (arr[1]()(40) != 42)
		return 4;
	if (h.fp()(40) != 42)
		return 5;
	if ((pick ? maker : fallback)()(40) != 42)
		return 6;
	if ((0 ? fallback : maker)()(40) != 42)
		return 7;

	return 42;
}
