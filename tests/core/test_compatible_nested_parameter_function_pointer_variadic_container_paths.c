typedef int (*variadic_fn_a)(int, ...);
typedef int (*variadic_fn_b)(int, ...);
typedef variadic_fn_a *variadic_ptr_a;
typedef variadic_fn_b *variadic_ptr_b;
typedef int (*outer_a)(variadic_ptr_a);
typedef int (*outer_b)(variadic_ptr_b);

struct holder {
	outer_b fp;
};

static int
add2(int x, ...)
{
	return x + 2;
}

static int
call_ptr(variadic_ptr_b pp)
{
	return (*pp)(40, 7);
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
	variadic_fn_a fa = add2;
	variadic_ptr_b pb = &fa;

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
