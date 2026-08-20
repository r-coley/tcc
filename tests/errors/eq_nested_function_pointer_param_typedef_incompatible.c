typedef int (*good_fn)(int);
typedef int (*bad_fn)(char);
typedef good_fn *good_fn_ptr;
typedef bad_fn *bad_fn_ptr;

int
target_int(int x)
{
	return x;
}

int
target_char(char x)
{
	return x;
}

int
main(void)
{
	good_fn g = target_int;
	bad_fn b = target_char;
	good_fn_ptr gp = &g;
	bad_fn_ptr bp = &b;

	return (gp == bp) ? 1 : 0;
}
