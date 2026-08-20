typedef int (*fn_a)(int);
typedef int (*fn_b)(int);
typedef fn_a *fn_a_ptr;
typedef fn_b *fn_b_ptr;
typedef int (*outer_a)(fn_a_ptr);
typedef int (*outer_b)(fn_b_ptr);

struct holder {
	outer_b fp;
};

static int
add2(int x)
{
	return x + 2;
}

static int
call_ptr(fn_b_ptr pp)
{
	return (*pp)(40);
}

outer_a gp;
outer_b gp = call_ptr;

extern outer_a ext_gp;
outer_b ext_gp = call_ptr;

int
main(void)
{
	outer_b local = gp;
	outer_b arr[2] = { [1] = gp };
	struct holder h = { gp };
	int pick = 1;
	outer_a fallback = gp;
	fn_a fa = add2;
	fn_b_ptr pb = &fa;

	if (gp(pb) != 42)
		return 1;
	if (ext_gp(pb) != 42)
		return 2;
	if (local(pb) != 42)
		return 3;
	if (arr[1](pb) != 42)
		return 4;
	if (h.fp(pb) != 42)
		return 5;
	if ((pick ? gp : fallback)(pb) != 42)
		return 6;
	if ((0 ? fallback : gp)(pb) != 42)
		return 7;

	return 42;
}
